import asyncio
import json
import os
import sys
import subprocess
from pathlib import Path
from typing import Optional, Set
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from pydantic import BaseModel
import uvicorn

BASE_DIR = Path(__file__).resolve().parent.parent
WEB_DIR = BASE_DIR / "web"
EXE_PATH = BASE_DIR / "web_bridge.exe"

app = FastAPI(title="Limit Order Book & Market Maker Engine")

class OrderRequest(BaseModel):
    side: str  # BUY or SELL
    price: int
    quantity: int
    order_type: str = "LIMIT"  # LIMIT or MARKET

class CancelRequest(BaseModel):
    order_id: int

class SimControlRequest(BaseModel):
    running: bool
    speed_ms: int = 200
    regime: str = "NORMAL"

class MMStrategyRequest(BaseModel):
    strategy: str  # NONE, BASELINE, AVELLANEDA_STOIKOV

class EngineBridge:
    def __init__(self, exe_path: Path):
        self.exe_path = exe_path
        self.proc: Optional[asyncio.subprocess.Process] = None
        self.lock = asyncio.Lock()
        self.latest_state: dict = {}
        self.connected_websockets: Set[WebSocket] = set()
        self.sim_running = False
        self.sim_speed_ms = 200
        self.sim_regime = "NORMAL"
        self.sim_task: Optional[asyncio.Task] = None

    async def start(self):
        if not self.exe_path.exists():
            raise FileNotFoundError(f"Bridge executable not found: {self.exe_path}")

        self.proc = await asyncio.create_subprocess_exec(
            str(self.exe_path),
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )
        # Fetch initial state
        await self.send_command("STATE")
        # Start background simulator loop
        self.sim_task = asyncio.create_task(self._sim_loop())

    async def send_command(self, cmd_str: str) -> dict:
        async with self.lock:
            if not self.proc or self.proc.returncode is not None:
                await self.start()

            assert self.proc.stdin is not None
            assert self.proc.stdout is not None

            self.proc.stdin.write((cmd_str.strip() + "\n").encode("utf-8"))
            await self.proc.stdin.drain()

            line_bytes = await self.proc.stdout.readline()
            line_str = line_bytes.decode("utf-8").strip()
            if not line_str:
                return self.latest_state

            try:
                state = json.loads(line_str)
                self.latest_state = state
                return state
            except json.JSONDecodeError as e:
                print(f"[Bridge] JSON Decode Error on line: {line_str}, err: {e}", file=sys.stderr)
                return self.latest_state

    async def broadcast_state(self):
        if not self.connected_websockets:
            return
        state_json = json.dumps(self.latest_state)
        dead_ws = set()
        for ws in list(self.connected_websockets):
            try:
                await ws.send_text(state_json)
            except Exception:
                dead_ws.add(ws)
        for ws in dead_ws:
            self.connected_websockets.discard(ws)

    async def _sim_loop(self):
        while True:
            try:
                if self.sim_running:
                    await self.send_command("SIM_STEP 1")
                    await self.broadcast_state()
                delay = max(20, self.sim_speed_ms) / 1000.0
                await asyncio.sleep(delay)
            except asyncio.CancelledError:
                break
            except Exception as e:
                print(f"[SimLoop] Error: {e}", file=sys.stderr)
                await asyncio.sleep(0.5)

    async def stop(self):
        if self.sim_task:
            self.sim_task.cancel()
        if self.proc and self.proc.returncode is None:
            try:
                self.proc.terminate()
                await self.proc.wait()
            except Exception:
                pass

bridge = EngineBridge(EXE_PATH)

@app.on_event("startup")
async def on_startup():
    await bridge.start()

@app.on_event("shutdown")
async def on_shutdown():
    await bridge.stop()

@app.get("/api/state")
async def get_state():
    return bridge.latest_state

@app.post("/api/order")
async def submit_order(order: OrderRequest):
    if order.order_type.upper() == "MARKET":
        cmd = f"SUBMIT_MARKET {order.side.upper()} {order.quantity}"
    else:
        cmd = f"SUBMIT_LIMIT {order.side.upper()} {order.price} {order.quantity}"
    state = await bridge.send_command(cmd)
    await bridge.broadcast_state()
    return state

@app.post("/api/cancel")
async def cancel_order(req: CancelRequest):
    cmd = f"CANCEL {req.order_id}"
    state = await bridge.send_command(cmd)
    await bridge.broadcast_state()
    return state

@app.post("/api/sim/control")
async def control_sim(req: SimControlRequest):
    bridge.sim_running = req.running
    bridge.sim_speed_ms = req.speed_ms
    if req.regime != bridge.sim_regime:
        bridge.sim_regime = req.regime
        await bridge.send_command(f"SET_REGIME {req.regime}")
    return {"status": "ok", "running": bridge.sim_running, "speed_ms": bridge.sim_speed_ms, "regime": bridge.sim_regime}

@app.post("/api/sim/step")
async def step_sim(count: int = 1):
    state = await bridge.send_command(f"SIM_STEP {count}")
    await bridge.broadcast_state()
    return state

@app.post("/api/mm/strategy")
async def set_strategy(req: MMStrategyRequest):
    state = await bridge.send_command(f"SET_MM {req.strategy.upper()}")
    await bridge.broadcast_state()
    return state

@app.post("/api/reset")
async def reset_engine():
    bridge.sim_running = False
    state = await bridge.send_command("RESET")
    await bridge.broadcast_state()
    return state

@app.websocket("/ws")
async def websocket_endpoint(ws: WebSocket):
    await ws.accept()
    bridge.connected_websockets.add(ws)
    try:
        # Send immediate initial state
        if bridge.latest_state:
            await ws.send_text(json.dumps(bridge.latest_state))

        while True:
            data_text = await ws.receive_text()
            try:
                msg = json.loads(data_text)
                action = msg.get("action", "").upper()

                if action == "GET_STATE":
                    await ws.send_text(json.dumps(bridge.latest_state))
                elif action == "SUBMIT_ORDER":
                    side = msg.get("side", "BUY").upper()
                    price = int(msg.get("price", 0))
                    qty = int(msg.get("quantity", 1))
                    otype = msg.get("order_type", "LIMIT").upper()
                    if otype == "MARKET":
                        cmd = f"SUBMIT_MARKET {side} {qty}"
                    else:
                        cmd = f"SUBMIT_LIMIT {side} {price} {qty}"
                    await bridge.send_command(cmd)
                    await bridge.broadcast_state()
                elif action == "CANCEL_ORDER":
                    oid = int(msg.get("order_id", 0))
                    await bridge.send_command(f"CANCEL {oid}")
                    await bridge.broadcast_state()
                elif action == "SIM_CONTROL":
                    bridge.sim_running = bool(msg.get("running", bridge.sim_running))
                    bridge.sim_speed_ms = int(msg.get("speed_ms", bridge.sim_speed_ms))
                    regime = msg.get("regime")
                    if regime and regime != bridge.sim_regime:
                        bridge.sim_regime = regime
                        await bridge.send_command(f"SET_REGIME {regime}")
                    await bridge.broadcast_state()
                elif action == "SIM_STEP":
                    count = int(msg.get("count", 1))
                    await bridge.send_command(f"SIM_STEP {count}")
                    await bridge.broadcast_state()
                elif action == "SET_MM":
                    strat = msg.get("strategy", "NONE")
                    await bridge.send_command(f"SET_MM {strat}")
                    await bridge.broadcast_state()
                elif action == "RESET":
                    bridge.sim_running = False
                    await bridge.send_command("RESET")
                    await bridge.broadcast_state()
            except Exception as e:
                print(f"[WS] Error processing client message: {e}", file=sys.stderr)
    except WebSocketDisconnect:
        bridge.connected_websockets.discard(ws)
    except Exception as e:
        bridge.connected_websockets.discard(ws)

# Mount static web directory
if WEB_DIR.exists():
    app.mount("/static", StaticFiles(directory=str(WEB_DIR)), name="static")

@app.get("/")
async def serve_index():
    index_file = WEB_DIR / "index.html"
    if index_file.exists():
        return FileResponse(index_file)
    return {"message": "Frontend files not yet created in web/ directory."}

if __name__ == "__main__":
    uvicorn.run("server:app", host="127.0.0.1", port=8000, reload=False)
