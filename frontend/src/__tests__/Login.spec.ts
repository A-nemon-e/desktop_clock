import { describe, it, expect } from 'vitest'

function validateLogin(username: string, password: string): { valid: boolean; error?: string } {
  if (!username.trim()) return { valid: false, error: 'Username required' }
  if (!password) return { valid: false, error: 'Password required' }
  return { valid: true }
}

describe('Login Validation', () => {
  it('rejects empty username', () => {
    const result = validateLogin('', 'password123')
    expect(result.valid).toBe(false)
  })

  it('rejects empty password', () => {
    const result = validateLogin('admin', '')
    expect(result.valid).toBe(false)
  })

  it('accepts valid credentials', () => {
    const result = validateLogin('admin', 'password123')
    expect(result.valid).toBe(true)
  })

  it('rejects whitespace-only username', () => {
    const result = validateLogin('   ', 'password123')
    expect(result.valid).toBe(false)
  })
})
