"use server";

import { exec } from 'child_process';
import { promisify } from 'util';
import path from 'path';
import { SimulationParameters, SimulationResult } from '@/lib/types';

const execAsync = promisify(exec);

export async function runSimulationAction(params: SimulationParameters): Promise<SimulationResult> {
  const binaryPath = path.resolve(process.cwd(), '../build/simulator');
  
  // Build arguments string
  const args = [
    '--json',
    `--sigma ${params.sigma_annual}`,
    `--gamma ${params.gamma}`,
    `--k ${params.k}`,
    `--A ${params.A}`,
    `--price ${params.initial_price}`,
    `--duration ${params.duration}`,
    `--tick ${params.market_tick_dt}`,
    `--seed ${params.seed}`,
    `--latency ${params.latency}`,
    `--maker-fee ${params.maker_fee}`,
    `--queue-base ${params.queue_position_base}`,
    `--toxicity-rho ${params.toxicity_rho}`,
    `--toxicity-drift ${params.toxicity_drift_impact}`,
    `--toxicity-intensity ${params.toxicity_intensity_impact}`
  ].join(' ');

  try {
    // We might need a larger maxBuffer because the JSON output can be large (e.g. 50MB for 15k events)
    // Actually, for 3600 seconds with tick=2, it's ~2000 events, JSON is a few MBs.
    const { stdout } = await execAsync(`${binaryPath} ${args}`, { maxBuffer: 1024 * 1024 * 50 });
    
    const result = JSON.parse(stdout) as SimulationResult;
    return result;
  } catch (error) {
    console.error('Failed to run simulation binary:', error);
    throw new Error('Simulation failed to run.');
  }
}
