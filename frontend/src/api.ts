import axios from 'axios'
import type {
  AuthUser,
  CardData,
  CardConfig,
  DeviceData,
  FirmwareData,
  UserData,
} from './types'

const api = axios.create({
  baseURL: '/api',
  withCredentials: true,
  headers: { 'Content-Type': 'application/json' },
})

// ── Auth ────────────────────────────────────────────────

export async function login(username: string, password: string) {
  const { data } = await api.post('/auth/login', { username, password })
  return data
}

export async function logout() {
  const { data } = await api.post('/auth/logout')
  return data
}

export async function getMe(): Promise<AuthUser> {
  const { data } = await api.get('/auth/me')
  return { id: data.id, username: data.username, is_admin: data.is_admin }
}

// ── Admin: Users ────────────────────────────────────────

export async function adminListUsers(skip = 0, limit = 50) {
  const { data } = await api.get('/admin/users', { params: { skip, limit } })
  return data as { users: UserData[]; total: number }
}

export async function adminCreateUser(username: string, password: string) {
  const { data } = await api.post('/admin/users', { username, password })
  return data as UserData
}

export async function adminDeleteUser(userId: string) {
  const { data } = await api.delete(`/admin/users/${userId}`)
  return data
}

// ── Admin: Devices ──────────────────────────────────────

export interface AdminDeviceItem {
  mac: string
  hw_rev: string
  fw_version: string | null
  ip: string | null
  region: string | null
  thin_mode: string
  last_seen: string | null
}

export async function adminListDevices(skip = 0, limit = 50) {
  const { data } = await api.get('/admin/devices', { params: { skip, limit } })
  return data as { devices: AdminDeviceItem[]; total: number }
}

// ── Admin: Firmware ─────────────────────────────────────

export async function adminCreateFirmware(hw_rev: string, version: string, changelog: string) {
  const { data } = await api.post('/admin/firmware', { hw_rev, version, changelog })
  return data as { message: string; id: string }
}

export async function adminUploadFirmwareFile(firmwareId: string, file: File) {
  const formData = new FormData()
  formData.append('file', file)
  const { data } = await api.post(`/admin/firmware/${firmwareId}/upload`, formData, {
    headers: { 'Content-Type': 'multipart/form-data' },
  })
  return data
}

// ── User: Devices ───────────────────────────────────────

export async function listMyDevices() {
  const { data } = await api.get('/user/devices')
  return data as { devices: DeviceData[] }
}

export async function bindDevice(mac: string) {
  const { data } = await api.post('/user/devices/bind', { mac })
  return data
}

// ── Device Info ─────────────────────────────────────────

export async function getDevice(mac: string) {
  const { data } = await api.get(`/device/${mac}`)
  return data as DeviceData
}

// ── Cards ───────────────────────────────────────────────

export async function getDeviceCards(mac: string) {
  const { data } = await api.get(`/device/${mac}/cards`)
  return data as { cards: CardData[] }
}

export interface CardSyncItem {
  bg_type: string
  fg_config: CardConfig | Record<string, never>
  duration: number
  rel_brightness: number
  position: number
}

export async function syncDeviceCards(mac: string, cards: CardSyncItem[], version: number) {
  const { data } = await api.post(`/device/${mac}/cards/sync`, { cards, device_mac: mac, version })
  return data
}
