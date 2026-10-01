// Professional Limit Order Book Trading Terminal Application
(function() {
  'use strict';

  // --- STATE ---
  let ws = null;
  let isConnected = false;
  let currentSide = 'BUY'; // BUY or SELL
  let simRunning = false;
  let soundEnabled = true;
  let priceHistory = []; // { time, mid, ref }
  let maxHistoryLength = 100;
  let lastTradeId = 0;

  // --- DOM ELEMENTS ---
  const headerRefPrice = document.getElementById('header-ref-price');
  const headerMidPrice = document.getElementById('header-mid-price');
  const headerSpread = document.getElementById('header-spread');
  const headerTotalOrders = document.getElementById('header-total-orders');
  const headerSeq = document.getElementById('header-seq');
  const connectionBadge = document.getElementById('connection-badge');
  const connectionText = document.getElementById('connection-text');

  const tabBuy = document.getElementById('tab-buy');
  const tabSell = document.getElementById('tab-sell');
  const inputPrice = document.getElementById('input-price');
  const inputQty = document.getElementById('input-qty');
  const notionalPreview = document.getElementById('notional-preview');
  const btnSubmitOrder = document.getElementById('btn-submit-order');
  const btnSubmitText = document.getElementById('btn-submit-text');
  const btnFillMid = document.getElementById('btn-fill-mid');
  const priceInputContainer = document.getElementById('price-input-container');

  const btnToggleSim = document.getElementById('btn-toggle-sim');
  const btnSimText = document.getElementById('btn-sim-text');
  const btnStepSim = document.getElementById('btn-step-sim');
  const simStatusBadge = document.getElementById('sim-status-badge');
  const simSpeedSlider = document.getElementById('sim-speed-slider');
  const simSpeedLabel = document.getElementById('sim-speed-label');
  const selectRegime = document.getElementById('select-regime');
  const selectStrategy = document.getElementById('select-strategy');
  const btnReset = document.getElementById('btn-reset');
  const btnSound = document.getElementById('btn-sound');

  const asksContainer = document.getElementById('asks-container');
  const bidsContainer = document.getElementById('bids-container');
  const domMidPrice = document.getElementById('dom-mid-price');
  const domSpread = document.getElementById('dom-spread');

  const tradesContainer = document.getElementById('trades-container');
  const tradeCountBadge = document.getElementById('trade-count-badge');
  const userOrdersContainer = document.getElementById('user-orders-container');

  const mmInventoryVal = document.getElementById('mm-inventory-val');
  const mmInventoryBar = document.getElementById('mm-inventory-bar');
  const mmRealizedPnl = document.getElementById('mm-realized-pnl');
  const mmUnrealizedPnl = document.getElementById('mm-unrealized-pnl');
  const mmTotalPnl = document.getElementById('mm-total-pnl');
  const mmReservationPrice = document.getElementById('mm-reservation-price');
  const mmVolatility = document.getElementById('mm-volatility');

  const canvas = document.getElementById('priceChart');
  const ctx = canvas.getContext('2d');

  // --- AUDIO SYNTHESIZER ---
  const audioCtx = (typeof window !== 'undefined' && (window.AudioContext || window.webkitAudioContext)) ? new (window.AudioContext || window.webkitAudioContext)() : null;
  function playSound(freq, duration = 0.04) {
    if (!soundEnabled || !audioCtx) return;
    try {
      if (audioCtx.state === 'suspended') audioCtx.resume();
      const osc = audioCtx.createOscillator();
      const gain = audioCtx.createGain();
      osc.type = 'sine';
      osc.frequency.setValueAtTime(freq, audioCtx.currentTime);
      gain.gain.setValueAtTime(0.04, audioCtx.currentTime);
      gain.gain.exponentialRampToValueAtTime(0.0001, audioCtx.currentTime + duration);
      osc.connect(gain);
      gain.connect(audioCtx.destination);
      osc.start();
      osc.stop(audioCtx.currentTime + duration);
    } catch (e) {}
  }

  // --- WEBSOCKET CONNECTION ---
  function connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;

    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      isConnected = true;
      connectionBadge.className = 'flex items-center gap-1.5 px-2 py-0.5 bg-[#12261e] border border-[#1a3d30] text-[#2ebd85] rounded-[2px]';
      connectionText.textContent = 'ONLINE';
      sendWS({ action: 'GET_STATE' });
    };

    ws.onmessage = (event) => {
      try {
        const state = JSON.parse(event.data);
        if (state.type === 'STATE') {
          updateUI(state);
        }
      } catch (err) {
        console.error('WS Parse error', err);
      }
    };

    ws.onclose = () => {
      isConnected = false;
      connectionBadge.className = 'flex items-center gap-1.5 px-2 py-0.5 bg-[#2d1216] border border-[#4a1c23] text-[#f6465d] rounded-[2px]';
      connectionText.textContent = 'OFFLINE';
      setTimeout(connectWebSocket, 1500);
    };

    ws.onerror = () => {
      ws.close();
    };
  }

  function sendWS(data) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(data));
    }
  }

  // --- UI UPDATE HANDLER ---
  function updateUI(state) {
    // 1. Header Metrics
    headerRefPrice.textContent = Number(state.ref_price).toLocaleString();
    headerSeq.textContent = Number(state.sequence).toLocaleString();
    headerTotalOrders.textContent = Number(state.total_orders).toLocaleString();

    if (state.has_mid) {
      headerMidPrice.textContent = Number(state.mid_price).toFixed(2);
      domMidPrice.textContent = Number(state.mid_price).toFixed(2);
    } else {
      headerMidPrice.textContent = state.ref_price ? Number(state.ref_price).toLocaleString() : '--';
      domMidPrice.textContent = '--';
    }

    if (state.has_spread) {
      headerSpread.textContent = `${state.spread}`;
      domSpread.textContent = `${state.spread}`;
    } else {
      headerSpread.textContent = '--';
      domSpread.textContent = '--';
    }

    // 2. Chart History
    const currentMid = state.has_mid ? state.mid_price : state.ref_price;
    priceHistory.push({
      time: Date.now(),
      mid: currentMid,
      ref: state.ref_price
    });
    if (priceHistory.length > maxHistoryLength) {
      priceHistory.shift();
    }
    renderChart();

    // 3. Render Order Book (DOM)
    renderDOM(state.bids || [], state.asks || []);

    // 4. Render Time & Sales
    renderTrades(state.trades || []);

    // 5. Render User Working Orders
    renderUserOrders(state.user_orders || []);

    // 6. Render Market Maker Metrics
    if (state.mm) {
      renderMMStats(state.mm);
    }
  }

  // --- RENDER DOM LADDER ---
  function renderDOM(bids, asks) {
    let maxVol = 1;
    for (const b of bids) if (b.volume > maxVol) maxVol = b.volume;
    for (const a of asks) if (a.volume > maxVol) maxVol = a.volume;

    // Asks (reversed: highest on top, best ask at bottom)
    const reversedAsks = [...asks].reverse();
    if (reversedAsks.length === 0) {
      asksContainer.innerHTML = '<div class="text-center text-[#5e6673] py-2 text-[11px]">No asks resting</div>';
    } else {
      let asksHtml = '';
      for (const ask of reversedAsks) {
        const depthPct = Math.min(100, Math.round((ask.volume / maxVol) * 100));
        asksHtml += `
          <div class="dom-row" onclick="window.setOrderPrice(${ask.price})">
            <div class="dom-bar-ask" style="width: ${depthPct}%;"></div>
            <div class="text-[#f6465d] font-semibold z-10">${Number(ask.price).toLocaleString()}</div>
            <div class="text-center text-[#eaecef] z-10">${Number(ask.volume).toLocaleString()}</div>
            <div class="text-right text-[#848e9c] z-10 text-[10px]">${ask.count}</div>
          </div>
        `;
      }
      asksContainer.innerHTML = asksHtml;
    }

    // Bids (best bid at top)
    if (bids.length === 0) {
      bidsContainer.innerHTML = '<div class="text-center text-[#5e6673] py-2 text-[11px]">No bids resting</div>';
    } else {
      let bidsHtml = '';
      for (const bid of bids) {
        const depthPct = Math.min(100, Math.round((bid.volume / maxVol) * 100));
        bidsHtml += `
          <div class="dom-row" onclick="window.setOrderPrice(${bid.price})">
            <div class="dom-bar-bid" style="width: ${depthPct}%;"></div>
            <div class="text-[#2ebd85] font-semibold z-10">${Number(bid.price).toLocaleString()}</div>
            <div class="text-center text-[#eaecef] z-10">${Number(bid.volume).toLocaleString()}</div>
            <div class="text-right text-[#848e9c] z-10 text-[10px]">${bid.count}</div>
          </div>
        `;
      }
      bidsContainer.innerHTML = bidsHtml;
    }
  }

  window.setOrderPrice = function(price) {
    inputPrice.value = price;
    updateNotional();
  };

  // --- RENDER TIME & SALES ---
  function renderTrades(trades) {
    tradeCountBadge.textContent = `${trades.length} TRADES`;
    if (trades.length === 0) {
      tradesContainer.innerHTML = '<div class="text-center text-[#5e6673] py-4 text-[11px]">No trades executed</div>';
      return;
    }

    if (trades.length > 0 && trades[0].trade_id > lastTradeId) {
      if (lastTradeId !== 0) {
        playSound(trades[0].side === 'BUY' ? 700 : 450, 0.03);
      }
      lastTradeId = trades[0].trade_id;
    }

    let html = '';
    for (const t of trades.slice(0, 40)) {
      const isBuy = t.side === 'BUY';
      const sideColor = isBuy ? 'text-[#2ebd85]' : 'text-[#f6465d]';
      const timeStr = new Date().toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' });

      html += `
        <div class="grid grid-cols-4 px-2 py-0.5 items-center hover:bg-[#1c2130] border-b border-[#1a1e2b]">
          <div class="text-[#5e6673] text-[10px]">${timeStr}</div>
          <div class="font-bold text-[10px] ${sideColor}">${t.side}</div>
          <div class="text-right font-semibold ${sideColor}">${Number(t.price).toLocaleString()}</div>
          <div class="text-right text-[#eaecef]">${Number(t.quantity).toLocaleString()}</div>
        </div>
      `;
    }
    tradesContainer.innerHTML = html;
  }

  // --- RENDER WORKING ORDERS ---
  function renderUserOrders(orders) {
    if (orders.length === 0) {
      userOrdersContainer.innerHTML = '<div class="text-center text-[#5e6673] py-4 text-[11px]">No open orders</div>';
      return;
    }

    let html = '';
    for (const o of orders) {
      const isBuy = o.side === 'BUY';
      const sideColor = isBuy ? 'text-[#2ebd85]' : 'text-[#f6465d]';

      html += `
        <div class="grid grid-cols-5 px-2 py-1 items-center bg-[#151924] border border-[#232838] rounded-[2px]">
          <div class="text-[#5e6673] text-[10px]">#${o.order_id}</div>
          <div class="font-bold text-[10px] ${sideColor}">${o.side}</div>
          <div class="text-right font-semibold text-[#eaecef]">${Number(o.price).toLocaleString()}</div>
          <div class="text-right text-[#848e9c]">${Number(o.quantity).toLocaleString()}</div>
          <div class="text-center">
            <button onclick="window.cancelOrder(${o.order_id})" class="px-1.5 py-0.5 bg-[#2d1216] border border-[#4a1c23] text-[#f6465d] hover:bg-[#3d181e] text-[10px] font-bold rounded-[2px]">
              Cancel
            </button>
          </div>
        </div>
      `;
    }
    userOrdersContainer.innerHTML = html;
  }

  window.cancelOrder = function(orderId) {
    sendWS({ action: 'CANCEL_ORDER', order_id: orderId });
    playSound(350, 0.04);
  };

  // --- RENDER MM METRICS ---
  function renderMMStats(mm) {
    const inv = mm.inventory;
    mmInventoryVal.textContent = `${inv > 0 ? '+' : ''}${inv} / ±100`;

    const pct = Math.max(0, Math.min(100, 50 + (inv / 100) * 50));
    mmInventoryBar.style.width = `${pct}%`;
    if (inv > 0) {
      mmInventoryBar.className = 'h-full bg-[#2ebd85]';
    } else if (inv < 0) {
      mmInventoryBar.className = 'h-full bg-[#f6465d]';
    } else {
      mmInventoryBar.className = 'h-full bg-[#848e9c]';
    }

    const fmtPnl = (val) => {
      const prefix = val >= 0 ? '+$' : '-$';
      return `${prefix}${Math.abs(val).toFixed(2)}`;
    };

    mmRealizedPnl.textContent = fmtPnl(mm.realized_pnl);
    mmRealizedPnl.className = `font-semibold ${mm.realized_pnl >= 0 ? 'text-[#2ebd85]' : 'text-[#f6465d]'}`;

    mmUnrealizedPnl.textContent = fmtPnl(mm.unrealized_pnl);
    mmUnrealizedPnl.className = `font-semibold ${mm.unrealized_pnl >= 0 ? 'text-[#2ebd85]' : 'text-[#f6465d]'}`;

    mmTotalPnl.textContent = fmtPnl(mm.total_pnl);
    mmTotalPnl.className = `font-semibold ${mm.total_pnl >= 0 ? 'text-[#2ebd85]' : 'text-[#f6465d]'}`;

    mmReservationPrice.textContent = mm.reservation_price > 0 ? mm.reservation_price.toFixed(2) : '--';
    mmVolatility.textContent = mm.volatility > 0 ? mm.volatility.toFixed(4) : '--';
  }

  // --- CANVAS CHART ---
  function resizeCanvas() {
    const rect = canvas.getBoundingClientRect();
    if (rect.width > 0 && rect.height > 0) {
      canvas.width = rect.width * window.devicePixelRatio;
      canvas.height = rect.height * window.devicePixelRatio;
      ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
    }
  }
  window.addEventListener('resize', () => {
    resizeCanvas();
    renderChart();
  });
  setTimeout(resizeCanvas, 50);

  function renderChart() {
    const rect = canvas.getBoundingClientRect();
    const width = rect.width;
    const height = rect.height;
    if (width <= 0 || height <= 0 || priceHistory.length < 2) return;

    ctx.clearRect(0, 0, width, height);

    let minP = Infinity, maxP = -Infinity;
    for (const pt of priceHistory) {
      if (pt.mid > 0) {
        if (pt.mid < minP) minP = pt.mid;
        if (pt.mid > maxP) maxP = pt.mid;
      }
      if (pt.ref > 0) {
        if (pt.ref < minP) minP = pt.ref;
        if (pt.ref > maxP) maxP = pt.ref;
      }
    }

    if (minP === Infinity || maxP === -Infinity) return;
    const padding = Math.max(4, (maxP - minP) * 0.12);
    minP -= padding;
    maxP += padding;
    const range = maxP - minP || 1;

    // Grid Lines & Price Axis
    ctx.strokeStyle = '#181d29';
    ctx.lineWidth = 1;
    const numGrid = 3;
    for (let i = 0; i <= numGrid; ++i) {
      const y = height - (i / numGrid) * (height - 18) - 9;
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(width - 50, y);
      ctx.stroke();

      const priceLevel = minP + (i / numGrid) * range;
      ctx.fillStyle = '#5e6673';
      ctx.font = '10px "JetBrains Mono"';
      ctx.textAlign = 'right';
      ctx.fillText(priceLevel.toFixed(1), width - 4, y + 3);
    }

    const getX = (idx) => (idx / (priceHistory.length - 1)) * (width - 55);
    const getY = (val) => height - 9 - ((val - minP) / range) * (height - 18);

    // Reference Price Line (Dashed Gold)
    ctx.setLineDash([3, 3]);
    ctx.strokeStyle = '#f0b90b';
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let i = 0; i < priceHistory.length; ++i) {
      const x = getX(i);
      const y = getY(priceHistory[i].ref);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
    ctx.setLineDash([]);

    // Mid Price Solid Line (Clean White)
    ctx.strokeStyle = '#eaecef';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    for (let i = 0; i < priceHistory.length; ++i) {
      const x = getX(i);
      const y = getY(priceHistory[i].mid);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();

    // Last Price Node
    const lastX = getX(priceHistory.length - 1);
    const lastY = getY(priceHistory[priceHistory.length - 1].mid);
    ctx.fillStyle = '#eaecef';
    ctx.beginPath();
    ctx.arc(lastX, lastY, 3, 0, Math.PI * 2);
    ctx.fill();
  }

  // --- ORDER ENTRY CONTROLS ---
  function updateNotional() {
    const price = parseFloat(inputPrice.value) || 0;
    const qty = parseFloat(inputQty.value) || 0;
    const notional = price * qty;
    notionalPreview.textContent = `Val: ${notional.toLocaleString()}`;
  }

  tabBuy.addEventListener('click', () => {
    currentSide = 'BUY';
    tabBuy.className = 'py-1 text-xs font-bold bg-[#2ebd85] text-[#0b0e14] rounded-[2px]';
    tabSell.className = 'py-1 text-xs font-bold text-[#848e9c] hover:text-white rounded-[2px]';
    btnSubmitOrder.className = 'btn-buy py-2 text-xs uppercase tracking-wide mono mt-1';
    btnSubmitText.textContent = 'Submit Buy Limit';
  });

  tabSell.addEventListener('click', () => {
    currentSide = 'SELL';
    tabSell.className = 'py-1 text-xs font-bold bg-[#f6465d] text-white rounded-[2px]';
    tabBuy.className = 'py-1 text-xs font-bold text-[#848e9c] hover:text-white rounded-[2px]';
    btnSubmitOrder.className = 'btn-sell py-2 text-xs uppercase tracking-wide mono mt-1';
    btnSubmitText.textContent = 'Submit Sell Limit';
  });

  document.querySelectorAll('input[name="orderType"]').forEach(radio => {
    radio.addEventListener('change', (e) => {
      const isMarket = e.target.value === 'MARKET';
      priceInputContainer.style.opacity = isMarket ? '0.35' : '1';
      inputPrice.disabled = isMarket;
      btnSubmitText.textContent = `Submit ${currentSide} ${e.target.value}`;
    });
  });

  document.getElementById('btn-price-plus').addEventListener('click', () => {
    inputPrice.value = (parseInt(inputPrice.value) || 10000) + 1;
    updateNotional();
  });
  document.getElementById('btn-price-minus').addEventListener('click', () => {
    inputPrice.value = Math.max(1, (parseInt(inputPrice.value) || 10000) - 1);
    updateNotional();
  });

  document.getElementById('btn-qty-plus').addEventListener('click', () => {
    inputQty.value = (parseInt(inputQty.value) || 1) + 1;
    updateNotional();
  });
  document.getElementById('btn-qty-minus').addEventListener('click', () => {
    inputQty.value = Math.max(1, (parseInt(inputQty.value) || 1) - 1);
    updateNotional();
  });

  document.querySelectorAll('.btn-qty-preset').forEach(btn => {
    btn.addEventListener('click', () => {
      inputQty.value = btn.getAttribute('data-qty');
      updateNotional();
    });
  });

  inputPrice.addEventListener('input', updateNotional);
  inputQty.addEventListener('input', updateNotional);

  btnFillMid.addEventListener('click', () => {
    const mid = parseFloat(domMidPrice.textContent);
    if (!isNaN(mid)) {
      inputPrice.value = Math.round(mid);
      updateNotional();
    }
  });

  btnSubmitOrder.addEventListener('click', () => {
    const otype = document.querySelector('input[name="orderType"]:checked').value;
    const price = parseInt(inputPrice.value) || 10000;
    const qty = parseInt(inputQty.value) || 1;

    sendWS({
      action: 'SUBMIT_ORDER',
      side: currentSide,
      price: price,
      quantity: qty,
      order_type: otype
    });

    playSound(currentSide === 'BUY' ? 550 : 380, 0.03);
  });

  // --- SIMULATION & STRATEGY CONTROLS ---
  btnToggleSim.addEventListener('click', () => {
    simRunning = !simRunning;
    if (simRunning) {
      btnSimText.textContent = 'Pause Sim';
      simStatusBadge.className = 'mono text-[10px] text-[#2ebd85] font-semibold';
      simStatusBadge.textContent = 'RUNNING';
    } else {
      btnSimText.textContent = 'Start Sim';
      simStatusBadge.className = 'mono text-[10px] text-[#848e9c]';
      simStatusBadge.textContent = 'PAUSED';
    }

    sendWS({
      action: 'SIM_CONTROL',
      running: simRunning,
      speed_ms: parseInt(simSpeedSlider.value),
      regime: selectRegime.value
    });
  });

  btnStepSim.addEventListener('click', () => {
    sendWS({ action: 'SIM_STEP', count: 1 });
    playSound(480, 0.02);
  });

  simSpeedSlider.addEventListener('input', (e) => {
    const val = e.target.value;
    simSpeedLabel.textContent = `${val} ms`;
    sendWS({
      action: 'SIM_CONTROL',
      running: simRunning,
      speed_ms: parseInt(val),
      regime: selectRegime.value
    });
  });

  selectRegime.addEventListener('change', (e) => {
    sendWS({
      action: 'SIM_CONTROL',
      running: simRunning,
      speed_ms: parseInt(simSpeedSlider.value),
      regime: e.target.value
    });
  });

  selectStrategy.addEventListener('change', (e) => {
    sendWS({
      action: 'SET_MM',
      strategy: e.target.value
    });
  });

  btnReset.addEventListener('click', () => {
    if (confirm('Reset engine and order book state?')) {
      sendWS({ action: 'RESET' });
      simRunning = false;
      btnSimText.textContent = 'Start Sim';
      simStatusBadge.className = 'mono text-[10px] text-[#848e9c]';
      simStatusBadge.textContent = 'PAUSED';
      priceHistory = [];
    }
  });

  btnSound.addEventListener('click', () => {
    soundEnabled = !soundEnabled;
    btnSound.textContent = `Audio: ${soundEnabled ? 'ON' : 'OFF'}`;
  });

  connectWebSocket();
})();
