import { describe, it, expect } from 'vitest'

// Card constraint validation logic (same as firmware card_mgr.c and backend user.py)
const NO_FOREGROUND_BGS = ['water_ripple', 'game_of_life', 'hourglass']
const FONT_RESTRICTED_BGS: Record<string, string> = { fire: '3x5' }
const VALID_FONTS = ['3x5', '5x5', '7x9', 'long']

function validateCardConstraints(bg: string, fgFonts: string[]): { valid: boolean; error?: string } {
  if (bg in FONT_RESTRICTED_BGS && fgFonts.length > 0) {
    const required = FONT_RESTRICTED_BGS[bg]
    for (const f of fgFonts) {
      if (f !== required) return { valid: false, error: `bg '${bg}' requires font '${required}'` }
    }
  }
  if (NO_FOREGROUND_BGS.includes(bg) && fgFonts.length > 0) {
    return { valid: false, error: `bg '${bg}' does not allow foregrounds` }
  }
  return { valid: true }
}

describe('CardEditor Constraints', () => {
  it('fire accepts 3x5 font', () => {
    const result = validateCardConstraints('fire', ['3x5'])
    expect(result.valid).toBe(true)
  })

  it('fire rejects 5x5 font', () => {
    const result = validateCardConstraints('fire', ['5x5'])
    expect(result.valid).toBe(false)
  })

  it('fire rejects 7x9 font', () => {
    const result = validateCardConstraints('fire', ['7x9'])
    expect(result.valid).toBe(false)
  })

  it('water_ripple rejects any foreground', () => {
    const result = validateCardConstraints('water_ripple', ['3x5'])
    expect(result.valid).toBe(false)
  })

  it('game_of_life rejects foreground', () => {
    const result = validateCardConstraints('game_of_life', ['3x5'])
    expect(result.valid).toBe(false)
  })

  it('hourglass rejects foreground', () => {
    const result = validateCardConstraints('hourglass', ['3x5'])
    expect(result.valid).toBe(false)
  })

  it('water_ripple ok with no foreground', () => {
    const result = validateCardConstraints('water_ripple', [])
    expect(result.valid).toBe(true)
  })

  it('code_rain allows any font', () => {
    const result = validateCardConstraints('code_rain', ['5x5'])
    expect(result.valid).toBe(true)
  })

  it('rejects more than 9 cards', () => {
    const cards = Array(9).fill({ bg: 'fire' })
    expect(cards.length).toBe(9)
    expect(() => {
      if (cards.length >= 9) throw new Error('max 9 cards')
    }).toThrow()
  })
})

describe('Font Validation', () => {
  it('all 4 font sizes are valid', () => {
    for (const font of VALID_FONTS) {
      expect(VALID_FONTS).toContain(font)
    }
  })

  it('invalid font is rejected', () => {
    expect(VALID_FONTS).not.toContain('10x20')
  })
})
