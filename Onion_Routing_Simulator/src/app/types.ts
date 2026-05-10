// src/app/types.ts

export interface Point {
  x: number;
  y: number;
}

export interface PacketState {
  id: string;
  path: Point[];
  type: "normal" | "syn" | "encrypted" | "corrupted";
  encryptionLayers?: number;
}

export interface LogEntry {
  type: "info" | "success" | "alert" | "error";
  message: string;
}