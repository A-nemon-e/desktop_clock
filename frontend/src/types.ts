// ============================================================
// Desktop Clock — Shared Types (mirrors firmware types.h)
// ============================================================

/** Background effect types (7 total) */
export const BG_TYPES = {
  FIRE: 'fire',
  CODE_RAIN: 'code_rain',
  WATER_RIPPLE: 'water_ripple',
  GAME_OF_LIFE: 'game_of_life',
  HOURGLASS: 'hourglass',
  PONG_CLOCK: 'pong_clock',
  WEATHER: 'weather',
} as const

export type BgType = (typeof BG_TYPES)[keyof typeof BG_TYPES]

export const BG_LABELS: Record<BgType, string> = {
  fire: 'Doom Fire',
  code_rain: 'Code Rain',
  water_ripple: 'Water Ripple',
  game_of_life: 'Game of Life',
  hourglass: 'Hourglass',
  pong_clock: 'Pong Clock',
  weather: 'Weather',
}

/** Backgrounds where foreground configuration is disabled */
export const BG_NO_FOREGROUND: BgType[] = ['water_ripple', 'game_of_life', 'hourglass']

/** Backgrounds using simplified toggles instead of font selector */
export const BG_SIMPLIFIED: BgType[] = ['pong_clock', 'weather']

/** Foreground display types (5 total) */
export const FG_TYPES = {
  CLOCK_HMS: 'hms',
  CLOCK_HM: 'hm',
  DATE_FULL: 'date_full',
  DATE_COMPACT: 'date_compact',
  TEMP_HUMID: 'temp_humid',
} as const

export type FgType = (typeof FG_TYPES)[keyof typeof FG_TYPES]

export const FG_LABELS: Record<FgType, string> = {
  hms: 'Clock HH:MM:SS',
  hm: 'Clock HH:MM',
  date_full: 'Date (Full)',
  date_compact: 'Date (Compact)',
  temp_humid: 'Temperature + Humidity',
}

/** Font sizes (4 total) */
export const FONT_SIZES = {
  '3x5': '3×5',
  '5x5': '5×5',
  '7x9': '7×9',
  long: 'Long',
} as const

export type FontSize = keyof typeof FONT_SIZES

/** A single foreground slot configuration */
export interface FgSlot {
  type: FgType
  font: FontSize
  duration_ms: number
}

/** A single card configuration */
export interface CardConfig {
  bg: BgType
  fgs: FgSlot[]
  fg_count: number
  relative_brightness: number // -128 to +127
  switch_interval: number // seconds, 0 = manual only
}

/** Card as returned by the backend API */
export interface CardData {
  id: string
  device_mac: string
  position: number
  bg_type: string
  fg_config: CardConfig | Record<string, never>
  duration: number
  rel_brightness: number
  created_at: string
  updated_at: string
}

/** Device as returned by the backend API */
export interface DeviceData {
  mac: string
  hw_rev: string
  fw_version: string | null
  ip: string | null
  region: string | null
  timezone: string | null
  owner_id: string | null
  backend_urls: Record<string, string>
  latitude: number | null
  longitude: number | null
  thin_mode: string
  last_seen: string
}

/** User as returned by the backend API */
export interface UserData {
  id: string
  username: string
  is_admin: boolean
  created_at: string
}

/** Firmware as returned by the backend API */
export interface FirmwareData {
  id: string
  hw_rev: string
  version: string
  file_path: string
  sha256: string
  changelog: string
  created_at: string
}

/** Current authenticated user info */
export interface AuthUser {
  id: string
  username: string
  is_admin: boolean
}

export const MAX_CARDS = 9
export const MAX_FG_PER_CARD = 5
