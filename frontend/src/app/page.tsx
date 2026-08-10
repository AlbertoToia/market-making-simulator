"use client";

import { useState, useEffect } from 'react';
import { SimulationHeader } from '@/components/SimulationHeader';
import { ControlPanel } from '@/components/ControlPanel';
import { ChartArea } from '@/components/ChartArea';
import { runSimulationAction } from '@/app/actions';
import { useReplayEngine } from '@/hooks/useReplayEngine';
import { SimulationParameters, SimulationResult } from '@/lib/types';

const DEFAULT_PARAMS: SimulationParameters = {
  sigma_annual: 0.20,
  gamma: 20.0,
  k: 1.5,
  A: 2.0,
  mu: 0.0,
  initial_price: 100.0,
  duration: 3600.0,
  market_tick_dt: 1.0, 
  seed: 42,
  latency: 0.05,
  maker_fee: 0.0001,
  queue_position_base: 100,
  toxicity_rho: 0.99,
  toxicity_drift_impact: 0.0,
  toxicity_intensity_impact: 0.5,
};

export default function Home() {
  const [params, setParams] = useState<SimulationParameters>(DEFAULT_PARAMS);
  const [simulation, setSimulation] = useState<SimulationResult | null>(null);
  const [loading, setLoading] = useState<boolean>(false);
  const [isMobileParamsOpen, setIsMobileParamsOpen] = useState<boolean>(false);

  // Replay Engine
  const {
    isPlaying,
    togglePlay,
    playbackSpeed,
    setPlaybackSpeed,
    currentTime,
    duration,
    reset,
    currentEvent
  } = useReplayEngine(simulation);

  // Initial load only
  useEffect(() => {
    runSimulation(params);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const runSimulation = async (newParams: SimulationParameters) => {
    setLoading(true);
    reset();
    setParams(newParams); // Update main app state
    try {
      const result = await runSimulationAction(newParams);
      setSimulation(result);
    } catch (err) {
      console.error(err);
      alert("Failed to run C++ simulation");
    } finally {
      setLoading(false);
    }
  };

  return (
    <main className="flex flex-col md:flex-row h-screen w-screen overflow-x-hidden overflow-y-auto md:overflow-hidden bg-white text-black font-sans relative">
      
      {loading && (
        <div className="absolute inset-0 bg-white/50 backdrop-blur-sm z-50 flex items-center justify-center font-mono text-sm tracking-widest uppercase">
          Running C++ Engine...
        </div>
      )}

      {/* Main Simulation Area */}
      <div className="flex-1 flex flex-col h-full border-r border-gray-200 min-w-0">
        <SimulationHeader 
          currentEvent={currentEvent}
          isPlaying={isPlaying}
          togglePlay={togglePlay}
          reset={reset}
          playbackSpeed={playbackSpeed}
          setPlaybackSpeed={setPlaybackSpeed}
          onOpenMobileParams={() => setIsMobileParamsOpen(true)}
        />
        
        {simulation && (
          <ChartArea 
            timeline={simulation.timeline}
            currentTime={currentTime}
            duration={duration}
            summary={simulation.summary}
          />
        )}

        <footer className="border-t border-gray-200 bg-white py-2 text-center text-[11px] sm:text-xs italic text-gray-500 font-sans shrink-0 px-2">
          Built by <a href="https://www.albertotoia.com" target="_blank" rel="noopener noreferrer" className="underline hover:text-black transition-colors not-italic font-medium">Alberto Toia</a> out of an incessant desire to build something stimulating.
        </footer>
      </div>

      {/* Control Panel Sidebar */}
      <ControlPanel 
        params={params}
        setParams={setParams}
        runSimulation={runSimulation}
        isMobileOpen={isMobileParamsOpen}
        onMobileClose={() => setIsMobileParamsOpen(false)}
      />

    </main>
  );
}
