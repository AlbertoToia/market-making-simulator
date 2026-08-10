import { clsx, type ClassValue } from 'clsx';
import { twMerge } from 'tailwind-merge';

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

// Formatting utilities for the trader UI
export function formatPrice(price: number, decimals: number = 2) {
  return price.toFixed(decimals);
}

export function formatVolume(vol: number) {
  if (Math.abs(vol) >= 1000) {
    return (vol / 1000).toFixed(1) + 'k';
  }
  return vol.toString();
}

export function formatTime(seconds: number) {
  const mins = Math.floor(seconds / 60);
  const secs = Math.floor(seconds % 60);
  const ms = Math.floor((seconds % 1) * 1000);
  return `${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}.${ms.toString().padStart(3, '0')}`;
}
