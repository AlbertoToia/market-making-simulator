"use client";

import { TimelineEntry, SimulationSummary } from '@/lib/types';
import {
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,
  AreaChart, Area
} from 'recharts';
import { formatPrice } from '@/lib/utils';
import { useMemo, useRef, useEffect } from 'react';

interface ChartAreaProps {
  timeline: TimelineEntry[];
  currentTime: number;
  duration: number;
  summary: SimulationSummary | null;
}

export function ChartArea({ timeline, currentTime, duration, summary }: ChartAreaProps) {
  const logContainerRef = useRef<HTMLDivElement>(null);
  
  if (timeline.length === 0) {
    return <div className="flex-grow flex items-center justify-center text-gray-400 font-mono text-sm">No simulation data available</div>;
  }

  // Downsample timeline for SVG rendering performance if timeline > 2000 points
  const displayTimeline = useMemo(() => {
    if (timeline.length <= 2000) return timeline;
    const step = Math.ceil(timeline.length / 2000);
    const sampled: TimelineEntry[] = [];
    for (let i = 0; i < timeline.length; i += step) {
      sampled.push(timeline[i]);
    }
    if (sampled[sampled.length - 1] !== timeline[timeline.length - 1]) {
      sampled.push(timeline[timeline.length - 1]);
    }
    return sampled;
  }, [timeline]);

  const minPrice = useMemo(() => Math.min(...displayTimeline.map(d => d.bid)) - 0.5, [displayTimeline]);
  const maxPrice = useMemo(() => Math.max(...displayTimeline.map(d => d.ask)) + 0.5, [displayTimeline]);

  // Live order / execution events up to current replay time
  const liveLogs = useMemo(() => {
    return timeline
      .filter(e => e.timestamp <= currentTime && e.event_type !== 'MARKET_UPDATE')
      .slice(-40);
  }, [timeline, currentTime]);

  useEffect(() => {
    if (logContainerRef.current) {
      logContainerRef.current.scrollTop = logContainerRef.current.scrollHeight;
    }
  }, [liveLogs]);

  // Calculate progress ratio (0 to 1) for the GPU cursor overlay
  const progressRatio = duration > 0 ? Math.min(1, Math.max(0, currentTime / duration)) : 0;

  const CustomTooltip = ({ active, payload, label }: any) => {
    if (active && payload && payload.length) {
      return (
        <div className="bg-white border border-black p-2 text-xs font-mono shadow-sm">
          <div className="text-gray-500 mb-1">Time: {Number(label).toFixed(2)}s</div>
          {payload.map((entry: any, index: number) => (
            <div key={index} style={{ color: entry.color }} className="flex justify-between gap-4">
              <span>{entry.name}:</span>
              <span>{typeof entry.value === 'number' ? formatPrice(entry.value, 3) : entry.value}</span>
            </div>
          ))}
        </div>
      );
    }
    return null;
  };

  return (
    <div className="flex-grow flex flex-col p-2 sm:p-4 gap-3 sm:gap-4 overflow-y-auto bg-white">
      
      {/* Price Chart */}
      <div className="min-h-[250px] sm:min-h-[300px] border border-gray-200 relative overflow-hidden flex-grow">
        <div className="absolute top-2 left-2 text-[11px] sm:text-xs font-bold uppercase tracking-widest text-black z-10 bg-white px-1">Market Quotes</div>
        
        {/* GPU Accelerated Cursor & Desaturation Overlay */}
        <div 
          className="absolute top-0 bottom-0 pointer-events-none z-20 bg-white/70 backdrop-blur-[0.5px]"
          style={{
            left: `calc(10px + ${progressRatio} * (100% - 60px))`,
            right: '50px'
          }}
        />
        <div 
          className="absolute top-0 bottom-0 w-[1.5px] bg-black pointer-events-none z-30"
          style={{
            left: `calc(10px + ${progressRatio} * (100% - 60px))`
          }}
        />

        <ResponsiveContainer width="100%" height="100%">
          <LineChart data={displayTimeline} margin={{ top: 20, right: 10, left: 10, bottom: 0 }}>
            <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="#f0f0f0" />
            <XAxis dataKey="timestamp" type="number" domain={[0, duration]} hide />
            <YAxis domain={[minPrice, maxPrice]} tick={{fontSize: 10, fontFamily: 'monospace'}} width={50} tickFormatter={v => v.toFixed(2)} orientation="right" axisLine={false} tickLine={false} />
            <Tooltip content={<CustomTooltip />} isAnimationActive={false} />
            
            <Line type="stepAfter" dataKey="ask" stroke="#dc2626" strokeWidth={1} dot={false} isAnimationActive={false} name="Ask" />
            <Line type="stepAfter" dataKey="mid_price" stroke="#000000" strokeWidth={1.5} dot={false} isAnimationActive={false} name="Mid" />
            <Line type="stepAfter" dataKey="bid" stroke="#16a34a" strokeWidth={1} dot={false} isAnimationActive={false} name="Bid" />
            <Line type="stepAfter" dataKey="reservation_price" stroke="#9ca3af" strokeDasharray="3 3" strokeWidth={1} dot={false} isAnimationActive={false} name="Reservation" />
          </LineChart>
        </ResponsiveContainer>
      </div>

      {/* Inventory Chart */}
      <div className="h-36 sm:h-44 border border-gray-200 relative overflow-hidden shrink-0">
        <div className="absolute top-2 left-2 text-[11px] sm:text-xs font-bold uppercase tracking-widest text-black z-10 bg-white px-1">Inventory</div>
        
        {/* GPU Accelerated Cursor & Desaturation Overlay */}
        <div 
          className="absolute top-0 bottom-0 pointer-events-none z-20 bg-white/70 backdrop-blur-[0.5px]"
          style={{
            left: `calc(10px + ${progressRatio} * (100% - 60px))`,
            right: '50px'
          }}
        />
        <div 
          className="absolute top-0 bottom-0 w-[1.5px] bg-black pointer-events-none z-30"
          style={{
            left: `calc(10px + ${progressRatio} * (100% - 60px))`
          }}
        />

        <ResponsiveContainer width="100%" height="100%">
          <AreaChart data={displayTimeline} margin={{ top: 20, right: 10, left: 10, bottom: 0 }}>
            <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="#f0f0f0" />
            <XAxis dataKey="timestamp" type="number" domain={[0, duration]} hide />
            <YAxis tick={{fontSize: 10, fontFamily: 'monospace'}} width={50} orientation="right" axisLine={false} tickLine={false} />
            <Tooltip content={<CustomTooltip />} isAnimationActive={false} />
            
            <Area type="stepAfter" dataKey="inventory" stroke="#000000" fill="#f3f4f6" strokeWidth={1} isAnimationActive={false} name="Position" />
          </AreaChart>
        </ResponsiveContainer>
      </div>

      {/* Bottom Section: Split 50/50 between Total PnL and Summary / Live Log Stream */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-3 sm:gap-4 h-auto lg:h-56 shrink-0">
        
        {/* Total PnL Chart */}
        <div className="border border-gray-200 relative h-44 lg:h-full overflow-hidden">
          <div className="absolute top-2 left-2 text-[11px] sm:text-xs font-bold uppercase tracking-widest text-black z-10 bg-white px-1">Total PnL</div>
          
          {/* GPU Accelerated Cursor & Desaturation Overlay */}
          <div 
            className="absolute top-0 bottom-0 pointer-events-none z-20 bg-white/70 backdrop-blur-[0.5px]"
            style={{
              left: `calc(10px + ${progressRatio} * (100% - 60px))`,
              right: '50px'
            }}
          />
          <div 
            className="absolute top-0 bottom-0 w-[1.5px] bg-black pointer-events-none z-30"
            style={{
              left: `calc(10px + ${progressRatio} * (100% - 60px))`
            }}
          />

          <ResponsiveContainer width="100%" height="100%">
            <LineChart data={displayTimeline} margin={{ top: 20, right: 10, left: 10, bottom: 0 }}>
              <CartesianGrid strokeDasharray="3 3" vertical={false} stroke="#f0f0f0" />
              <XAxis dataKey="timestamp" type="number" domain={[0, duration]} hide />
              <YAxis tick={{fontSize: 10, fontFamily: 'monospace'}} width={50} tickFormatter={v => v.toFixed(0)} orientation="right" axisLine={false} tickLine={false} />
              <Tooltip content={<CustomTooltip />} isAnimationActive={false} />
              
              <Line type="stepAfter" dataKey="total_pnl" stroke="#000000" strokeWidth={1.5} dot={false} isAnimationActive={false} name="PnL" />
            </LineChart>
          </ResponsiveContainer>
        </div>

        {/* Summary & Live Execution Log Window */}
        <div className="border border-gray-200 bg-white flex flex-col h-60 lg:h-full overflow-hidden">
          <div className="flex justify-between items-center px-3 py-1.5 border-b border-gray-200 bg-gray-50 text-xs font-bold uppercase tracking-wider">
            <span>Summary & Execution Stream</span>
            <span className="text-[10px] text-gray-500 font-mono font-normal">Sync: {currentTime.toFixed(1)}s</span>
          </div>

          <div className="flex-1 grid grid-cols-2 divide-x divide-gray-200 overflow-hidden">
            {/* Left: Summary Numbers */}
            <div className="p-3 overflow-y-auto font-mono text-xs flex flex-col justify-between gap-1">
              {summary ? (
                <>
                  <SummaryRow label="Net PnL" value={`$${formatPrice(summary.final_pnl)}`} color={summary.final_pnl >= 0 ? "text-green-600 font-bold" : "text-red-600 font-bold"} />
                  <SummaryRow label="Gross PnL" value={`$${formatPrice(summary.gross_pnl)}`} />
                  <SummaryRow label="Fees Paid" value={`$${formatPrice(summary.total_fees)}`} color="text-red-600" />
                  <SummaryRow label="Total Return" value={`${formatPrice(summary.total_return * 100)}%`} />
                  <SummaryRow label="Vol (Annual)" value={`${formatPrice(summary.realized_volatility * 100)}%`} />
                  <SummaryRow label="Max DD" value={`-$${formatPrice(summary.max_drawdown)}`} color="text-red-600" />
                  <SummaryRow label="Avg Spread" value={formatPrice(summary.average_spread, 4)} />
                  <SummaryRow label="Total Fills" value={summary.total_fills.toString()} />
                </>
              ) : (
                <div className="text-gray-400 text-center py-4">No summary data</div>
              )}
            </div>

            {/* Right: Live Execution Log Stream */}
            <div className="p-2 overflow-y-auto font-mono text-[11px] flex flex-col gap-1.5 bg-gray-50/40" ref={logContainerRef}>
              {liveLogs.length === 0 ? (
                <div className="text-gray-400 text-center py-6 italic text-[11px]">No execution events at t={currentTime.toFixed(1)}s</div>
              ) : (
                liveLogs.map((entry, idx) => (
                  <div key={idx} className="flex items-center justify-between gap-2 border-b border-gray-100 pb-1 text-[11px]">
                    <span className="text-gray-400 font-normal">{entry.timestamp.toFixed(1)}s</span>
                    {entry.event_type === 'QUOTE_HIT_BUY' && (
                      <span className="text-green-700 font-semibold bg-green-50 px-1.5 py-0.5 rounded border border-green-200/50">
                        BUY {entry.execution_quantity} @ ${formatPrice(entry.execution_price, 2)}
                      </span>
                    )}
                    {entry.event_type === 'QUOTE_HIT_SELL' && (
                      <span className="text-red-700 font-semibold bg-red-50 px-1.5 py-0.5 rounded border border-red-200/50">
                        SELL {entry.execution_quantity} @ ${formatPrice(entry.execution_price, 2)}
                      </span>
                    )}
                    {entry.event_type === 'QUOTE_UPDATE' && (
                      <span className="text-gray-600 bg-gray-100 px-1.5 py-0.5 rounded">
                        Quotes Updated
                      </span>
                    )}
                  </div>
                ))
              )}
            </div>
          </div>
        </div>

      </div>

    </div>
  );
}

function SummaryRow({ label, value, color = "text-black" }: { label: string, value: string | number, color?: string }) {
  return (
    <div className="flex justify-between items-center">
      <span className="text-gray-500 font-sans text-[11px] uppercase tracking-wider">{label}</span>
      <span className={`font-mono ${color}`}>{value}</span>
    </div>
  );
}
