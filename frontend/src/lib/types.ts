export interface SimulationParameters {
  sigma_annual: number;
  gamma: number;
  k: number;
  A: number;
  mu: number;
  initial_price: number;
  duration: number;
  market_tick_dt: number;
  seed: number;
  latency: number;
  maker_fee: number;
  queue_position_base: number;
  toxicity_rho: number;
  toxicity_drift_impact: number;
  toxicity_intensity_impact: number;
}

export type EventType = 'MARKET_UPDATE' | 'QUOTE_HIT_BUY' | 'QUOTE_HIT_SELL' | 'QUOTE_UPDATE';

export interface TimelineEntry {
  // Timing
  timestamp: number;
  event_type: EventType;

  // Market
  mid_price: number;
  log_return: number;
  toxicity: number;

  // Quotes
  bid: number;
  ask: number;
  reservation_price: number;
  spread: number;
  bid_distance: number;
  ask_distance: number;
  queue_ahead_bid: number;
  queue_ahead_ask: number;

  // Intensities
  lambda_bid: number;
  lambda_ask: number;

  // Portfolio
  inventory: number;
  inventory_notional: number;
  cash: number;
  realized_pnl: number;
  unrealized_pnl: number;
  total_pnl: number;

  // Execution
  execution_price: number;
  execution_quantity: number;
  fee_paid: number;
}

export interface SimulationSummary {
  final_pnl: number;
  gross_pnl: number;
  total_fees: number;
  total_return: number;
  realized_volatility: number;
  sharpe_ratio: number;
  max_drawdown: number;
  max_drawdown_pct: number;

  max_inventory: number;
  min_inventory: number;
  average_inventory: number;
  inventory_std: number;

  total_fills: number;
  buy_fills: number;
  sell_fills: number;
  total_volume: number;

  average_spread: number;
  average_bid_distance: number;
  average_ask_distance: number;

  total_events: number;
  simulation_duration: number;
}

export interface SimulationResult {
  parameters: SimulationParameters;
  timeline: TimelineEntry[];
  summary: SimulationSummary;
}
