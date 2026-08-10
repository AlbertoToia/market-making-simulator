import { SimulationParameters, SimulationResult, TimelineEntry, EventType, SimulationSummary } from './types';

// Simple Box-Muller transform for normal distribution
function randomNormal(mean = 0, stdev = 1) {
  const u = 1 - Math.random(); 
  const v = Math.random();
  const z = Math.sqrt(-2.0 * Math.log(u)) * Math.cos(2.0 * Math.PI * v);
  return z * stdev + mean;
}

export function generateMockSimulation(params: SimulationParameters): SimulationResult {
  const timeline: TimelineEntry[] = [];
  
  // Downsample target: we don't want 15,000 events in the UI for a smooth replay,
  // we'll aim for ~500-1000 events total by increasing the market tick and order arrival slightly for the mock.
  const TRADING_SECONDS_PER_YEAR = 252 * 6.5 * 3600;
  const sigmaPerSec = params.sigma_annual / Math.sqrt(TRADING_SECONDS_PER_YEAR);
  const driftPerSec = params.mu / TRADING_SECONDS_PER_YEAR;

  let t = 0.0;
  let midPrice = params.initial_price;
  let inventory = 0;
  let cash = 0;
  let realizedPnl = 0;
  let avgCost = 0;
  
  let nextMarketTick = params.market_tick_dt;

  const computeQuotes = (mid: number, inv: number) => {
    const variance = (mid * sigmaPerSec) ** 2;
    const r = mid - (inv * params.gamma * variance);
    const delta = (params.gamma * variance) + (2.0 / params.gamma) * Math.log(1.0 + (params.gamma / params.k));
    return {
      bid: r - delta / 2,
      ask: r + delta / 2,
      r,
      spread: delta
    };
  };

  const processExecution = (side: 'BUY' | 'SELL', qty: number, execPrice: number) => {
    if (side === 'BUY') {
      cash -= qty * execPrice;
      if (inventory >= 0) {
        avgCost = (avgCost * inventory + execPrice * qty) / (inventory + qty);
      } else {
        const closing = Math.min(qty, -inventory);
        realizedPnl += closing * (avgCost - execPrice);
        if (inventory + qty > 0) avgCost = execPrice;
        if (inventory + qty === 0) avgCost = 0;
      }
      inventory += qty;
    } else {
      cash += qty * execPrice;
      if (inventory <= 0) {
        avgCost = inventory === 0 ? execPrice : (avgCost * -inventory + execPrice * qty) / (-inventory + qty);
      } else {
        const closing = Math.min(qty, inventory);
        realizedPnl += closing * (execPrice - avgCost);
        if (inventory - qty < 0) avgCost = execPrice;
        if (inventory - qty === 0) avgCost = 0;
      }
      inventory -= qty;
    }
  };

  const computeUnrealized = (mid: number) => {
    if (inventory === 0) return 0;
    return inventory > 0 ? inventory * (mid - avgCost) : (-inventory) * (avgCost - mid);
  };

  while (t < params.duration) {
    const quotes = computeQuotes(midPrice, inventory);
    
    // Intensities
    const deltaBid = Math.max(0, midPrice - quotes.bid);
    const deltaAsk = Math.max(0, quotes.ask - midPrice);
    const lambdaBid = params.A * Math.exp(-params.k * deltaBid);
    const lambdaAsk = params.A * Math.exp(-params.k * deltaAsk);

    // Sample exponentials
    const dtBid = lambdaBid > 0 ? -Math.log(1 - Math.random()) / lambdaBid : Infinity;
    const dtAsk = lambdaAsk > 0 ? -Math.log(1 - Math.random()) / lambdaAsk : Infinity;
    const dtMarket = nextMarketTick - t;

    const dtNext = Math.min(dtBid, dtAsk, dtMarket);
    t += dtNext;
    if (t > params.duration) break;

    let eventType: EventType;
    let prevMid = midPrice;
    let execPrice = 0;
    let execQty = 0;

    if (dtNext === dtMarket) {
      eventType = 'MARKET_UPDATE';
      const z = randomNormal();
      midPrice *= Math.exp((driftPerSec - 0.5 * sigmaPerSec * sigmaPerSec) * params.market_tick_dt + sigmaPerSec * Math.sqrt(params.market_tick_dt) * z);
      nextMarketTick += params.market_tick_dt;
    } else if (dtNext === dtBid) {
      eventType = 'QUOTE_HIT_BUY';
      execPrice = quotes.bid;
      execQty = 1;
      processExecution('BUY', execQty, execPrice);
    } else {
      eventType = 'QUOTE_HIT_SELL';
      execPrice = quotes.ask;
      execQty = 1;
      processExecution('SELL', execQty, execPrice);
    }

    const unrealizedPnl = computeUnrealized(midPrice);
    const totalPnl = realizedPnl + unrealizedPnl;

    timeline.push({
      timestamp: t,
      event_type: eventType,
      mid_price: midPrice,
      log_return: (prevMid > 0 && midPrice > 0 && eventType === 'MARKET_UPDATE') ? Math.log(midPrice / prevMid) : 0,
      bid: quotes.bid,
      ask: quotes.ask,
      reservation_price: quotes.r,
      spread: quotes.spread,
      bid_distance: Math.max(0, midPrice - quotes.bid),
      ask_distance: Math.max(0, quotes.ask - midPrice),
      lambda_bid: lambdaBid,
      lambda_ask: lambdaAsk,
      inventory,
      inventory_notional: inventory * midPrice,
      cash,
      realized_pnl: realizedPnl,
      unrealized_pnl: unrealizedPnl,
      total_pnl: totalPnl,
      execution_price: 0,
      execution_quantity: 0,
      toxicity: 0,
      queue_ahead_bid: 0,
      queue_ahead_ask: 0,
      fee_paid: 0,
    });
  }

  const finalPnl = timeline[timeline.length - 1]?.total_pnl || 0;
  
  // Create a minimal summary object
  const summary: SimulationSummary = {
    final_pnl: finalPnl,
    gross_pnl: finalPnl,
    total_fees: 0,
    total_return: finalPnl / 1000,
    realized_volatility: params.sigma_annual,
    sharpe_ratio: 3000,
    max_drawdown: 0,
    max_drawdown_pct: 0,
    max_inventory: Math.max(...timeline.map(e => e.inventory)),
    min_inventory: Math.min(...timeline.map(e => e.inventory)),
    average_inventory: timeline.reduce((acc, e) => acc + e.inventory, 0) / timeline.length,
    inventory_std: 0,
    total_fills: timeline.filter(e => e.event_type !== 'MARKET_UPDATE').length,
    buy_fills: timeline.filter(e => e.event_type === 'QUOTE_HIT_BUY').length,
    sell_fills: timeline.filter(e => e.event_type === 'QUOTE_HIT_SELL').length,
    total_volume: timeline.filter(e => e.event_type !== 'MARKET_UPDATE').length,
    average_spread: timeline.reduce((acc, e) => acc + e.spread, 0) / timeline.length,
    average_bid_distance: 0,
    average_ask_distance: 0,
    total_events: timeline.length,
    simulation_duration: t
  };

  return { parameters: params, timeline, summary };
}
