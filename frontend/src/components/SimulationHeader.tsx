"use client";

import { TimelineEntry } from '@/lib/types';
import { formatPrice, formatTime } from '@/lib/utils';
import { Play, Square, RotateCcw, SlidersHorizontal } from 'lucide-react';
import { cn } from '@/lib/utils';

interface SimulationHeaderProps {
  currentEvent: TimelineEntry | null;
  isPlaying: boolean;
  togglePlay: () => void;
  reset: () => void;
  playbackSpeed: number;
  setPlaybackSpeed: (speed: number) => void;
  onOpenMobileParams?: () => void;
}

export function SimulationHeader({
  currentEvent,
  isPlaying,
  togglePlay,
  reset,
  playbackSpeed,
  setPlaybackSpeed,
  onOpenMobileParams,
}: SimulationHeaderProps) {
  
  const mid = currentEvent?.mid_price || 0;
  const pnl = currentEvent?.total_pnl || 0;
  const inventory = currentEvent?.inventory || 0;
  const time = currentEvent?.timestamp || 0;

  return (
    <header className="border-b border-gray-200 bg-white px-3 sm:px-4 py-2.5 flex flex-col md:flex-row items-stretch md:items-center justify-between gap-3 sticky top-0 z-10">
      
      {/* Left: Branding & Playback */}
      <div className="flex items-center justify-between md:justify-start gap-3 sm:gap-6">
        <div className="font-bold tracking-tight text-xs sm:text-sm uppercase shrink-0">
          MM SIM <span className="text-gray-400 lowercase font-normal">by</span> <span className="font-serif italic font-normal text-sm sm:text-base text-gray-800 normal-case">α t</span>
        </div>
        
        <div className="flex items-center gap-1.5 sm:gap-2 border-l border-gray-200 pl-3 sm:pl-6">
          <button onClick={togglePlay} className="p-1 hover:bg-gray-100 transition-colors" title={isPlaying ? "Pause" : "Play"}>
            {isPlaying ? <Square size={15} className="fill-black" /> : <Play size={15} className="fill-black" />}
          </button>
          <button onClick={reset} className="p-1 hover:bg-gray-100 transition-colors" title="Reset">
            <RotateCcw size={15} />
          </button>
          
          <div className="flex items-center gap-0.5 sm:gap-1 ml-1 sm:ml-4 bg-gray-50 p-0.5 sm:p-1 border border-gray-200 text-[11px] sm:text-xs">
            {[1, 5, 20, 100].map(speed => (
              <button
                key={speed}
                onClick={() => setPlaybackSpeed(speed)}
                className={cn("px-1.5 sm:px-2 py-0.5", playbackSpeed === speed ? "bg-black text-white" : "hover:bg-gray-200 text-gray-600")}
              >
                {speed}x
              </button>
            ))}
          </div>
        </div>

        {/* Mobile Parameters Toggle Button */}
        {onOpenMobileParams && (
          <button 
            onClick={onOpenMobileParams}
            className="md:hidden flex items-center gap-1 bg-black text-white text-xs px-2.5 py-1 font-medium hover:bg-gray-800 transition-colors ml-auto"
          >
            <SlidersHorizontal size={13} />
            <span>Params</span>
          </button>
        )}
      </div>

      {/* Right: Live Metrics Grid */}
      <div className="grid grid-cols-4 gap-2 sm:gap-4 md:flex md:items-center md:gap-8 font-mono text-xs sm:text-sm border-t md:border-t-0 pt-2 md:pt-0 border-gray-100">
        <div className="flex flex-col items-start md:items-end">
          <span className="text-[9px] sm:text-[10px] text-gray-400 uppercase tracking-wider font-sans">Time</span>
          <span className="font-medium text-xs sm:text-sm">{formatTime(time)}</span>
        </div>
        
        <div className="flex flex-col items-start md:items-end">
          <span className="text-[9px] sm:text-[10px] text-gray-400 uppercase tracking-wider font-sans">Mid Price</span>
          <span className="font-medium text-xs sm:text-sm">{formatPrice(mid, 2)}</span>
        </div>

        <div className="flex flex-col items-start md:items-end">
          <span className="text-[9px] sm:text-[10px] text-gray-400 uppercase tracking-wider font-sans">Inventory</span>
          <span className={cn("font-medium text-xs sm:text-sm", inventory > 0 ? "text-green-600" : inventory < 0 ? "text-red-600" : "text-black")}>
            {inventory > 0 ? '+' : ''}{inventory}
          </span>
        </div>

        <div className="flex flex-col items-start md:items-end">
          <span className="text-[9px] sm:text-[10px] text-gray-400 uppercase tracking-wider font-sans">Total PnL</span>
          <span className={cn("font-medium text-xs sm:text-sm", pnl > 0 ? "text-green-600" : pnl < 0 ? "text-red-600" : "text-black")}>
            {pnl > 0 ? '+' : ''}{formatPrice(pnl, 2)}
          </span>
        </div>
      </div>
      
    </header>
  );
}
