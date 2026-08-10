"use client";

import { useState, useEffect, useRef, useCallback } from 'react';
import { SimulationResult, TimelineEntry } from '@/lib/types';

export function useReplayEngine(simulation: SimulationResult | null) {
  const [isPlaying, setIsPlaying] = useState(false);
  const [playbackSpeed, setPlaybackSpeed] = useState(1);
  const [currentTime, setCurrentTime] = useState(0);
  
  const lastUpdateRef = useRef<number>(0);
  const animationRef = useRef<number>(0);

  const duration = simulation?.summary.simulation_duration || 0;

  const togglePlay = useCallback(() => {
    setIsPlaying(prev => !prev);
  }, []);

  const reset = useCallback(() => {
    setCurrentTime(0);
    setIsPlaying(false);
  }, []);

  useEffect(() => {
    if (!isPlaying || !simulation) {
      cancelAnimationFrame(animationRef.current);
      lastUpdateRef.current = 0;
      return;
    }

    const loop = (timestamp: number) => {
      if (!lastUpdateRef.current) lastUpdateRef.current = timestamp;
      const dtReal = (timestamp - lastUpdateRef.current) / 1000; // seconds elapsed in real life
      
      setCurrentTime(prevTime => {
        const nextTime = prevTime + dtReal * playbackSpeed;
        if (nextTime >= duration) {
          setIsPlaying(false);
          return duration;
        }
        return nextTime;
      });
      
      lastUpdateRef.current = timestamp;
      animationRef.current = requestAnimationFrame(loop);
    };

    animationRef.current = requestAnimationFrame(loop);

    return () => cancelAnimationFrame(animationRef.current);
  }, [isPlaying, playbackSpeed, simulation, duration]);

  // Derived state: what is the last event that occurred before or at currentTime?
  const visibleTimeline = simulation 
    ? simulation.timeline.filter(e => e.timestamp <= currentTime) 
    : [];
    
  const currentEvent = visibleTimeline.length > 0 
    ? visibleTimeline[visibleTimeline.length - 1] 
    : null;

  return {
    isPlaying,
    togglePlay,
    playbackSpeed,
    setPlaybackSpeed,
    currentTime,
    setCurrentTime,
    duration,
    reset,
    visibleTimeline,
    currentEvent
  };
}
