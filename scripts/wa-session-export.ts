#!/usr/bin/env bun
// ─────────────────────────────────────────────────────────────────────────────
//  wa-session-export.ts
//  Converts a Nothing Browser session export (.json) to a Baileys-ready
//  auth_info_multi.json file that Baileys can use to connect without QR scan.
//
//  Usage:
//    bun run wa-session-export.ts ./sessions/whatsapp-2026-03-15.json
//    bun run wa-session-export.ts ./sessions/last-session.json --output ./baileys-auth/
//
//  Install:
//    bun add @whiskeysockets/baileys
// ─────────────────────────────────────────────────────────────────────────────

import { readFileSync, writeFileSync, mkdirSync, existsSync } from 'fs'
import { join, dirname } from 'path'

// ── Types ─────────────────────────────────────────────────────────────────────
interface NBSession {
    saved_at:  string
    url:       string
    nb_version:string
    captures:  NBCapture[]
    ws_frames: NBWSFrame[]
    cookies:   NBCookie[]
    storage:   {
        localStorage:   Record<string, string>
        sessionStorage: Record<string, string>
    }
}

interface NBCapture {
    method:     string
    url:        string
    status:     string
    type:       string
    reqHeaders: string
    body:       string
}

interface NBWSFrame {
    url:       string
    direction: string
    data:      string
    binary:    boolean
    timestamp: string
}

interface NBCookie {
    name:     string
    value:    string
    domain:   string
    path:     string
    httpOnly: boolean
    secure:   boolean
    expires:  string
}

interface WASessionReport {
    savedAt:       string
    sessionUrl:    string
    keysFound:     string[]
    keysMissing:   string[]
    cookieCount:   number
    wsFrameCount:  number
    captureCount:  number
    baileysReady:  boolean
    baileysFile?:  string
    rawKeys:       Record<string, string>
    wsHandshake:   WAHandshakeInfo[]
    waDecodedFrames: WADecodedFrame[]
}

interface WAHandshakeInfo {
    frameIndex: number
    direction:  string
    sizeBytes:  number
    hexPreview: string
    type:       string
}

interface WADecodedFrame {
    direction: string
    timestamp: string
    data:      any
}

// ── WA localStorage key definitions ─────────────────────────────────────────
const WA_KEYS = {
    // Critical for Baileys session restore
    CRITICAL: [
        'WANoiseKey',          // Noise protocol keypair
        'WASignedIdentityKey', // Signed identity keypair
        'WARegistrationId',    // Device registration ID
        'WAAdvSecretKey',      // ADV secret key
        'WAToken1',            // Auth token 1
        'WAToken2',            // Auth token 2
    ],
    // Important but sometimes derivable
    IMPORTANT: [
        'WASecretBundle',      // Full secret bundle
        'WABrowserId',         // Browser identifier
        'WAIdentityKey',       // Identity keypair
        'WASignedPreKey',      // Signed prekey
        'WAPreKeys',           // One-time prekeys
        'WAKeepAliveToken',    // Session keep-alive
        'WABusinessProfile',   // Profile info (has JID)
        'WAUserName',          // Display name
    ],
    // Informational
    INFO: [
        'WAVersion',           // WA web version
        'WALastConnect',       // Last connection time
        'WAChat',              // Chat list cache
        'WAChatDB',            // Chat database
    ]
}

// ── Parse arguments ───────────────────────────────────────────────────────────
const args = process.argv.slice(2)
if (args.length === 0) {
    console.log(`
Nothing Browser → WhatsApp Session Exporter
─────────────────────────────────────────────
Usage:
  bun run wa-session-export.ts <session.json> [options]

Options:
  --output <dir>    Output directory (default: ./wa-output/)
  --baileys         Also generate Baileys auth_info file
  --report          Print detailed analysis report
  --frames          Show WA decoded frames from hook

Example:
  bun run wa-session-export.ts last-session.json --baileys --report
`)
    process.exit(0)
}

const sessionFile  = args[0]
const outputDir    = args.includes('--output')
    ? args[args.indexOf('--output') + 1]
    : './wa-output'
const genBaileys   = args.includes('--baileys')
const showReport   = args.includes('--report')
const showFrames   = args.includes('--frames')

// ── Load session ──────────────────────────────────────────────────────────────
if (!existsSync(sessionFile)) {
    console.error(`❌  File not found: ${sessionFile}`)
    console.error(`    Run Nothing Browser, go to web.whatsapp.com,`)
    console.error(`    then: Session → Save as WhatsApp Session`)
    process.exit(1)
}

console.log(`\n📂  Loading: ${sessionFile}`)
const session: NBSession = JSON.parse(readFileSync(sessionFile, 'utf8'))
console.log(`✓   Saved at:  ${session.saved_at}`)
console.log(`✓   URL:       ${session.url}`)
console.log(`✓   Captures:  ${session.captures?.length || 0}`)
console.log(`✓   WS frames: ${session.ws_frames?.length || 0}`)
console.log(`✓   Cookies:   ${session.cookies?.length || 0}`)

const ls = session.storage?.localStorage || {}

// ── Extract WA keys ───────────────────────────────────────────────────────────
console.log('\n── WhatsApp Session Keys ────────────────────────────────')

const keysFound:   string[] = []
const keysMissing: string[] = []
const rawKeys:     Record<string, string> = {}

// Check all WA key categories
const allWAKeys = [
    ...WA_KEYS.CRITICAL,
    ...WA_KEYS.IMPORTANT,
    ...WA_KEYS.INFO
]

for (const key of allWAKeys) {
    const val = ls[key]
    if (val) {
        keysFound.push(key)
        rawKeys[key] = val
        const preview = val.length > 60 ? val.slice(0, 60) + '...' : val
        const isCritical = WA_KEYS.CRITICAL.includes(key)
        console.log(`  ${isCritical ? '🔑' : '  '} ${key.padEnd(25)} ${preview}`)
    } else {
        keysMissing.push(key)
    }
}

// Also check for WA_DECODED frames from our hook
const waDecodedFrames: WADecodedFrame[] = session.ws_frames
    ?.filter(f => f.direction === 'WA_DECODED' || f.direction === 'WA_PLAIN' || f.direction === 'WA_INFO')
    ?.map(f => {
        let data: any = f.data
        try { data = JSON.parse(f.data) } catch(e) {}
        return { direction: f.direction, timestamp: f.timestamp, data }
    }) || []

// Check for WA keys captured via our hook in storage captures
const hookKeys = ls['WA_ALL_KEYS']
if (hookKeys) {
    console.log('\n  📡 WA hook captured additional keys:')
    try {
        const hk = JSON.parse(hookKeys)
        Object.entries(hk).forEach(([k, v]: [string, any]) => {
            if (!rawKeys[k]) {
                rawKeys[k] = v
                keysFound.push(k)
                console.log(`     ${k}: ${String(v).slice(0, 60)}`)
            }
        })
    } catch(e) {}
}

const criticalFound   = WA_KEYS.CRITICAL.filter(k => rawKeys[k])
const criticalMissing = WA_KEYS.CRITICAL.filter(k => !rawKeys[k])
const baileysReady    = criticalFound.length >= 4  // need at least 4 critical keys

console.log(`\n  Critical keys: ${criticalFound.length}/${WA_KEYS.CRITICAL.length}`)
if (criticalMissing.length > 0)
    console.log(`  Missing critical: ${criticalMissing.join(', ')}`)

// ── Analyze WS frames ─────────────────────────────────────────────────────────
console.log('\n── WebSocket Frames ─────────────────────────────────────')

const wsHandshake: WAHandshakeInfo[] = []

session.ws_frames?.forEach((frame, i) => {
    if (frame.binary && frame.data && !frame.data.startsWith('[')) {
        const raw = Buffer.from(frame.data, 'base64')
        const hex = raw.slice(0, 8).toString('hex').match(/.{2}/g)!.join(' ')

        // Determine frame type
        let type = i < 3 ? 'HANDSHAKE (plain protobuf)' : 'ENCRYPTED (Noise Protocol)'

        wsHandshake.push({
            frameIndex: i,
            direction:  frame.direction,
            sizeBytes:  raw.length,
            hexPreview: hex,
            type
        })

        if (i < 5) {  // show first 5 frames
            console.log(`  Frame ${i}: ${frame.direction.padEnd(12)} ${raw.length}b  ${hex}  (${type})`)
        }
    } else if (frame.direction.startsWith('WA_')) {
        console.log(`  Frame ${i}: ${frame.direction.padEnd(12)} [WA Hook Decoded]`)
    }
})

// ── Analyze cookies ───────────────────────────────────────────────────────────
console.log('\n── Cookies ──────────────────────────────────────────────')
const waCookies = session.cookies?.filter(c =>
    c.domain.includes('whatsapp') || c.domain.includes('wa.me')
) || []
console.log(`  WhatsApp cookies: ${waCookies.length}`)
waCookies.slice(0, 10).forEach(c => {
    console.log(`  ${c.name.padEnd(30)} ${c.value.slice(0, 40)}`)
})

// ── Generate Baileys auth file ────────────────────────────────────────────────
let baileysFile: string | undefined

if (baileysReady || genBaileys) {
    console.log('\n── Generating Baileys Auth File ─────────────────────────')
    mkdirSync(outputDir, { recursive: true })

    // Parse known key formats
    const parseKey = (key: string): any => {
        if (!rawKeys[key]) return null
        try {
            const parsed = JSON.parse(rawKeys[key])
            return parsed
        } catch(e) {
            return rawKeys[key]
        }
    }

    // Build Baileys creds format
    // This matches Baileys' AuthenticationCreds interface
    const creds: Record<string, any> = {
        noiseKey:          parseKey('WANoiseKey'),
        signedIdentityKey: parseKey('WASignedIdentityKey') || parseKey('WAIdentityKey'),
        signedPreKey:      parseKey('WASignedPreKey'),
        registrationId:    parseKey('WARegistrationId'),
        advSecretKey:      rawKeys['WAAdvSecretKey'] || '',
        nextPreKeyId:      0,
        firstUnuploadedPreKeyId: 0,
        serverHasPreKeys: true,
        account:           parseKey('WASecretBundle'),
        platform:          'web',

        // Try to get JID from profile
        me: (() => {
            const profile = parseKey('WABusinessProfile')
            if (profile?.id) return { id: profile.id, name: profile.displayName }
            const name = rawKeys['WAUserName']
            return name ? { id: '', name } : null
        })(),

        // Tokens
        routingInfo:    null,
        lastAccountSyncTimestamp: 0,
        myAppStateKeyId: null,
        firstAppStateSync: false,
        accountSyncCounter: 0,
        processedHistoryMessages: [],
        accountSettings: { unarchiveChats: false },
    }

    // Keys store
    const keys: Record<string, any> = {
        'pre-key':    {},
        'session':    {},
        'sender-key': {},
        'app-state-sync-key': {},
        'app-state-sync-version': {},
        'sender-key-memory': {}
    }

    // Parse prekeys if available
    const preKeysRaw = parseKey('WAPreKeys')
    if (preKeysRaw && typeof preKeysRaw === 'object') {
        Object.entries(preKeysRaw).forEach(([id, key]) => {
            keys['pre-key'][id] = key
        })
    }

    const authInfo = { creds, keys }

    // Write to output
    baileysFile = join(outputDir, 'auth_info_multi.json')
    writeFileSync(baileysFile, JSON.stringify(authInfo, null, 2))
    console.log(`  ✓ Written: ${baileysFile}`)

    // Also write raw keys for debugging
    const rawFile = join(outputDir, 'wa_raw_keys.json')
    writeFileSync(rawFile, JSON.stringify(rawKeys, null, 2))
    console.log(`  ✓ Raw keys: ${rawFile}`)

    // Write cookies in Baileys format
    if (waCookies.length > 0) {
        const cookieFile = join(outputDir, 'wa_cookies.json')
        writeFileSync(cookieFile, JSON.stringify(waCookies, null, 2))
        console.log(`  ✓ Cookies: ${cookieFile}`)
    }

    // Write WA decoded frames if we have them
    if (waDecodedFrames.length > 0) {
        const framesFile = join(outputDir, 'wa_decoded_frames.json')
        writeFileSync(framesFile, JSON.stringify(waDecodedFrames, null, 2))
        console.log(`  ✓ Decoded frames: ${framesFile}`)
    }
}

// ── Show WA decoded frames ────────────────────────────────────────────────────
if (showFrames && waDecodedFrames.length > 0) {
    console.log('\n── WA Decoded Frames (from hook) ────────────────────────')
    waDecodedFrames.forEach((f, i) => {
        console.log(`\n  Frame ${i} [${f.direction}] @ ${f.timestamp}`)
        console.log(JSON.stringify(f.data, null, 4)
            .split('\n').map(l => '    ' + l).join('\n'))
    })
}

// ── Report ────────────────────────────────────────────────────────────────────
if (showReport) {
    const report: WASessionReport = {
        savedAt:         session.saved_at,
        sessionUrl:      session.url,
        keysFound,
        keysMissing,
        cookieCount:     session.cookies?.length || 0,
        wsFrameCount:    session.ws_frames?.length || 0,
        captureCount:    session.captures?.length || 0,
        baileysReady,
        baileysFile,
        rawKeys,
        wsHandshake,
        waDecodedFrames
    }

    const reportFile = join(outputDir, 'wa_analysis_report.json')
    mkdirSync(outputDir, { recursive: true })
    writeFileSync(reportFile, JSON.stringify(report, null, 2))
    console.log(`\n── Report saved: ${reportFile}`)
}

// ── Summary ───────────────────────────────────────────────────────────────────
console.log('\n── Summary ──────────────────────────────────────────────')
console.log(`  Session URL:       ${session.url}`)
console.log(`  WA keys found:     ${keysFound.length}`)
console.log(`  WS frames:         ${session.ws_frames?.length || 0}`)
console.log(`  WA decoded frames: ${waDecodedFrames.length}`)
console.log(`  Baileys ready:     ${baileysReady ? '✅  YES' : '⚠️   PARTIAL — scan QR first'}`)

if (!baileysReady) {
    console.log(`
  To get a complete session:
    1. Open Nothing Browser
    2. Go to web.whatsapp.com
    3. Scan QR code and wait for full load
    4. Click DECODE WA in the WS tab
    5. Wait 10 seconds for keys to populate
    6. Session → Save as WhatsApp Session
    7. Run this script again on the new file
`)
}

if (baileysFile) {
    console.log(`
  Use with Baileys:
  ─────────────────
  import makeWASocket, { useMultiFileAuthState } from '@whiskeysockets/baileys'
  
  const { state, saveCreds } = await useMultiFileAuthState('${outputDir}')
  const sock = makeWASocket({ auth: state })
  sock.ev.on('creds.update', saveCreds)
`)
}

console.log('────────────────────────────────────────────────────────\n')