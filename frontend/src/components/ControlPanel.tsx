"use client";

import { SimulationParameters } from '@/lib/types';
import { Settings, X } from 'lucide-react';
import { useState, useEffect } from 'react';
import { cn } from '@/lib/utils';

interface ControlPanelProps {
  params: SimulationParameters;
  setParams: (p: SimulationParameters) => void;
  runSimulation: (params: SimulationParameters) => void;
  isMobileOpen?: boolean;
  onMobileClose?: () => void;
}

export function ControlPanel({ params, setParams, runSimulation, isMobileOpen = false, onMobileClose }: ControlPanelProps) {
  
  // Local state for draft parameters so we don't trigger re-renders on the main app
  const [draft, setDraft] = useState<SimulationParameters>(params);

  // Sync draft if params prop changes from outside (e.g. initial load)
  useEffect(() => {
    setDraft(params);
  }, [params]);

  const updateParam = (key: keyof SimulationParameters, value: string) => {
    setDraft({ ...draft, [key]: Number(value) });
  };

  const handleRun = (p: SimulationParameters) => {
    runSimulation(p);
    if (onMobileClose) onMobileClose();
  };

  return (
    <>
      {/* Mobile Dark Backdrop Overlay */}
      {isMobileOpen && (
        <div 
          onClick={onMobileClose}
          className="fixed inset-0 bg-black/50 z-40 md:hidden backdrop-blur-xs transition-opacity"
        />
      )}

      {/* Main Panel Container */}
      <div className={cn(
        "w-80 border-l border-gray-200 bg-gray-50 flex flex-col overflow-y-auto shrink-0 transition-transform duration-200 ease-in-out z-50",
        // Desktop style
        "md:relative md:translate-x-0 md:flex md:h-full",
        // Mobile style (Fixed slide-over drawer)
        "fixed top-0 bottom-0 right-0 h-full shadow-2xl md:shadow-none",
        isMobileOpen ? "translate-x-0" : "translate-x-full md:translate-x-0"
      )}>
        
        <div className="p-4 border-b border-gray-200 flex items-center justify-between text-sm font-medium uppercase tracking-wide bg-white md:bg-transparent sticky top-0 z-10">
          <div className="flex items-center gap-2">
            <Settings size={14} />
            <span>Parameters</span>
          </div>
          {onMobileClose && (
            <button 
              onClick={onMobileClose}
              className="md:hidden p-1 text-gray-500 hover:text-black transition-colors"
            >
              <X size={18} />
            </button>
          )}
        </div>

        <div className="p-4 flex flex-col gap-4 flex-grow">
          <InputRow label="Initial Price" value={draft.initial_price} onChange={v => updateParam('initial_price', v)} step="1" />
          <InputRow label="Vol (Annual %)" value={draft.sigma_annual * 100} onChange={v => updateParam('sigma_annual', (Number(v)/100).toString())} step="1" />
          <InputRow label="Risk Aversion (γ)" value={draft.gamma} onChange={v => updateParam('gamma', v)} step="1" />
          <InputRow label="Liquidity (k)" value={draft.k} onChange={v => updateParam('k', v)} step="0.1" />
          <InputRow label="Intensity (A)" value={draft.A} onChange={v => updateParam('A', v)} step="0.1" />
          
          <div className="my-2 border-t border-gray-200"></div>
          <div className="text-xs uppercase tracking-wide font-sans text-gray-500">Execution Realism</div>
          
          <InputRow label="Latency (s)" value={draft.latency} onChange={v => updateParam('latency', v)} step="0.01" />
          <InputRow label="Maker Fee" value={draft.maker_fee} onChange={v => updateParam('maker_fee', v)} step="0.0001" />
          <InputRow label="LOB Queue Base" value={draft.queue_position_base} onChange={v => updateParam('queue_position_base', v)} step="10" />
          <InputRow label="Toxicity Impact" value={draft.toxicity_intensity_impact} onChange={v => updateParam('toxicity_intensity_impact', v)} step="0.1" />

          <div className="my-2 border-t border-gray-200"></div>

          <InputRow label="Duration (s)" value={draft.duration} onChange={v => updateParam('duration', v)} step="60" />
          <InputRow label="Random Seed" value={draft.seed} onChange={v => updateParam('seed', v)} step="1" />

          <button 
            onClick={() => handleRun(draft)}
            className="mt-4 w-full bg-black text-white text-sm font-medium py-2.5 hover:bg-gray-800 transition-colors"
          >
            Run New Simulation
          </button>

          <button 
            onClick={() => {
              const newSeed = Math.floor(Math.random() * 100000);
              const newDraft = { ...draft, seed: newSeed };
              setDraft(newDraft);
              handleRun(newDraft);
            }}
            className="w-full bg-gray-200 text-black text-sm font-medium py-2.5 hover:bg-gray-300 transition-colors"
          >
            Randomize Seed & Run
          </button>
        </div>

      </div>
    </>
  );
}

function InputRow({ label, value, onChange, step }: { label: string, value: number, onChange: (v: string) => void, step: string }) {
  return (
    <div className="flex flex-col gap-1">
      <label className="text-xs text-gray-500 uppercase font-sans tracking-wide">{label}</label>
      <input 
        type="number" 
        step={step}
        value={value} 
        onChange={e => onChange(e.target.value)}
        className="w-full border-b border-gray-300 bg-transparent px-0 py-1 text-sm font-mono focus:outline-none focus:border-black transition-colors [appearance:textfield] [&::-webkit-outer-spin-button]:appearance-none [&::-webkit-inner-spin-button]:appearance-none"
      />
    </div>
  );
}
