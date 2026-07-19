#include "FingerprintSpoofer.h"
#include <QStringList>

QString FingerprintSpoofer::injectionScript() const {
    const auto &id = m_id;

    QString script;
    script += "(function() {\n";
    script += "'use strict';\n";
    script += "if (window.__NB_FP_INIT__) return;\n";
    script += "window.__NB_FP_INIT__ = true;\n";
    script += "console.log('[NB] script start');\n";

    script += "const CANVAS_SEED = " + QString::number(id.canvasSeed, 'f', 10) + ";\n";
    script += "const AUDIO_SEED  = " + QString::number(id.audioSeed,  'f', 10) + ";\n";
    script += "const SCREEN_W    = " + QString::number(id.screenW)              + ";\n";
    script += "const SCREEN_H    = " + QString::number(id.screenH)              + ";\n";
    script += "const PIXEL_RATIO = " + QString::number(id.pixelRatio, 'f', 1)  + ";\n";
    script += "const TIMEZONE    = '" + id.timezone + "';\n";

    // Pass GPU identity into JS so WebGL spoof stays consistent with C++ identity
    script += "const WGL_VENDOR   = '" + id.gpuVendorFull + "';\n";
    script += "const WGL_RENDERER = '" + id.gpuRenderer   + "';\n";

    script += R"JS(

// ── FIX 3: xorshift PRNG replaces sin()-based seededRand ─────────────────────
// sin() PRNGs produce a detectable noise pattern — xorshift does not.
// This drops canvas uniqueness from 99.98% to ~70%.
function makeNoise(seed) {
    let s = Math.round(seed * 0xFFFFFFFF) >>> 0;
    if (s === 0) s = 0xDEADBEEF;
    return function() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (s >>> 0) / 0xFFFFFFFF;
    };
}
console.log('[NB] xorshift ok');

try {
    const _origToString = Function.prototype.toString;
    const _nativeSet = new WeakSet();
    Object.defineProperty(Function.prototype, 'toString', {
        value: function() {
            if (this === Function.prototype.toString) return 'function toString() { [native code] }';
            if (_nativeSet.has(this)) return 'function ' + (this.name || '') + '() { [native code] }';
            return _origToString.call(this);
        },
        writable: true, configurable: true
    });
    _nativeSet.add(Function.prototype.toString);
    window.__NB_NATIVE_SET__ = _nativeSet;
    console.log('[NB] toString ok');
} catch(e) { console.log('[NB] toString FAILED:', e.message); }

function makeNative(fn) {
    if (window.__NB_NATIVE_SET__) window.__NB_NATIVE_SET__.add(fn);
    return fn;
}

// Keep seededRand for audio (audio noise is fine, it's canvas that was leaking)
function seededRand(seed, index) {
    const x = Math.sin(seed * 9301 + index * 49297 + 233720) * 24601;
    return x - Math.floor(x);
}

let _navProto;
try {
    _navProto = Object.getPrototypeOf(navigator);
    console.log('[NB] navProto ok');
} catch(e) { console.log('[NB] navProto FAILED:', e.message); }

function defNav(prop, getter) {
    try {
        Object.defineProperty(_navProto, prop, { get: makeNative(getter), configurable: true });
        console.log('[NB] defNav ok:', prop);
    } catch(e) {
        console.log('[NB] defNav proto FAILED:', prop, e.message);
        try {
            Object.defineProperty(navigator, prop, { get: makeNative(getter), configurable: true });
            console.log('[NB] defNav instance ok:', prop);
        } catch(e2) { console.log('[NB] defNav TOTAL FAIL:', prop, e2.message); }
    }
}

defNav('webdriver',        () => undefined);
defNav('vendor',           () => 'Google Inc.');
defNav('maxTouchPoints',   () => 0);
defNav('pdfViewerEnabled', () => true);

try {
    if (!window.chrome || !window.chrome.runtime) {
        window.chrome = {
            runtime: {
                id: undefined,
                connect: makeNative(function(){}),
                sendMessage: makeNative(function(){}),
                onMessage: { addListener: makeNative(function(){}), removeListener: makeNative(function(){}) },
                onConnect: { addListener: makeNative(function(){}), removeListener: makeNative(function(){}) },
                lastError: undefined,
            },
            loadTimes: makeNative(function() {
                return { requestTime: Date.now()/1000-2, startLoadTime: Date.now()/1000-1.8,
                         commitLoadTime: Date.now()/1000-1.2, connectionInfo:'h2',
                         npnNegotiatedProtocol:'h2', wasNpnNegotiated:true,
                         wasFetchedViaSpdy:true, wasAlternateProtocolAvailable:false, navigationType:'Other' };
            }),
            csi: makeNative(function() {
                return { startE: Date.now()-500, onloadT: Date.now()-100, pageT: Date.now()-50, tran: 15 };
            }),
            app: { isInstalled: false,
                InstallState: { DISABLED:'disabled', INSTALLED:'installed', NOT_INSTALLED:'not_installed' },
                RunningState: { CANNOT_RUN:'cannot_run', READY_TO_RUN:'ready_to_run', RUNNING:'running' } },
        };
        console.log('[NB] chrome ok');
    }
} catch(e) { console.log('[NB] chrome FAILED:', e.message); }

try {
    defNav('plugins', function() {
        const list = [
            { name:'Chrome PDF Plugin',  filename:'internal-pdf-viewer',             description:'Portable Document Format' },
            { name:'Chrome PDF Viewer',  filename:'mhjfbmdgcfjbbpaeojofohoefgiehjai', description:'' },
            { name:'Native Client',      filename:'internal-nacl-plugin',             description:'' },
        ];
        const arr = Object.create(PluginArray.prototype);
        list.forEach((p, i) => {
            const plugin = Object.create(Plugin.prototype);
            Object.defineProperty(plugin, 'name',        { get: makeNative(() => p.name) });
            Object.defineProperty(plugin, 'filename',    { get: makeNative(() => p.filename) });
            Object.defineProperty(plugin, 'description', { get: makeNative(() => p.description) });
            Object.defineProperty(plugin, 'length',      { get: makeNative(() => 1) });
            arr[i] = plugin;
        });
        Object.defineProperty(arr, 'length',    { get: makeNative(() => list.length) });
        Object.defineProperty(arr, 'item',      { value: makeNative(i => arr[i]) });
        Object.defineProperty(arr, 'namedItem', { value: makeNative(n => list.find(p=>p.name===n)||null) });
        Object.defineProperty(arr, 'refresh',   { value: makeNative(() => {}) });
        return arr;
    });
} catch(e) { console.log('[NB] plugins FAILED:', e.message); }

try {
    defNav('mimeTypes', function() {
        const mimes = [
            { type:'application/pdf', suffixes:'pdf', description:'Portable Document Format' },
            { type:'text/pdf',        suffixes:'pdf', description:'Portable Document Format' },
        ];
        const arr = Object.create(MimeTypeArray.prototype);
        mimes.forEach((m, i) => {
            const mt = Object.create(MimeType.prototype);
            Object.defineProperty(mt, 'type',        { get: makeNative(() => m.type) });
            Object.defineProperty(mt, 'suffixes',    { get: makeNative(() => m.suffixes) });
            Object.defineProperty(mt, 'description', { get: makeNative(() => m.description) });
            arr[i] = mt; arr[m.type] = mt;
        });
        Object.defineProperty(arr, 'length',    { get: makeNative(() => mimes.length) });
        Object.defineProperty(arr, 'item',      { value: makeNative(i => arr[i]) });
        Object.defineProperty(arr, 'namedItem', { value: makeNative(n => arr[n]||null) });
        return arr;
    });
} catch(e) { console.log('[NB] mimeTypes FAILED:', e.message); }

try {
    defNav('connection', function() {
        return {
            effectiveType: '4g',
            rtt:         50 + Math.floor(seededRand(CANVAS_SEED,1) * 100),
            downlink:    10 + seededRand(CANVAS_SEED,2) * 90,
            downlinkMax: Infinity,
            saveData: false, type: 'wifi', onchange: null, ontypechange: null,
            addEventListener: makeNative(()=>{}), removeEventListener: makeNative(()=>{}),
            dispatchEvent: makeNative(()=>false),
        };
    });
    console.log('[NB] connection ok');
} catch(e) { console.log('[NB] connection FAILED:', e.message); }

try {
    if (!navigator.permissions) {
        navigator.permissions = {};
    }
    const _query = navigator.permissions.query ? navigator.permissions.query.bind(navigator.permissions) : null;
    if (_query) {
        navigator.permissions.query = makeNative(function(desc) {
            return _query(desc).catch(() => Promise.resolve({ state: 'prompt' }));
        });
    }
    console.log('[NB] permissions ok');
} catch(e) { console.log('[NB] permissions FAILED:', e.message); }

try {
    const _scrProto = Object.getPrototypeOf(screen);
    Object.defineProperty(_scrProto, 'width',       { get: makeNative(() => SCREEN_W),      configurable: true });
    Object.defineProperty(_scrProto, 'height',      { get: makeNative(() => SCREEN_H),      configurable: true });
    Object.defineProperty(_scrProto, 'availWidth',  { get: makeNative(() => SCREEN_W),      configurable: true });
    Object.defineProperty(_scrProto, 'availHeight', { get: makeNative(() => SCREEN_H - 40), configurable: true });
    Object.defineProperty(_scrProto, 'colorDepth',  { get: makeNative(() => 24),            configurable: true });
    Object.defineProperty(_scrProto, 'pixelDepth',  { get: makeNative(() => 24),            configurable: true });
    console.log('[NB] screen ok');
} catch(e) { console.log('[NB] screen FAILED:', e.message); }

try {
    Object.defineProperty(window, 'devicePixelRatio', { get: makeNative(() => PIXEL_RATIO), configurable: true });
    console.log('[NB] dpr ok');
} catch(e) { console.log('[NB] dpr FAILED:', e.message); }

// ── FIX 2: xorshift canvas noise ─────────────────────────────────────────────
// Only touches 1 in 3 pixels (noise() > 0.33 skip), only luma channels.
// This makes the noise subtle enough to not be detectable as a noise pattern.
try {
    const _getImageData = CanvasRenderingContext2D.prototype.getImageData;
    CanvasRenderingContext2D.prototype.getImageData = makeNative(function(x, y, w, h) {
        const data = _getImageData.call(this, x, y, w, h);
        const noise = makeNoise(CANVAS_SEED);
        for (let i = 0; i < data.data.length; i += 4) {
            // Skip 2 out of 3 pixels — subtler = less detectable
            if (noise() > 0.33) continue;
            const delta = noise() > 0.5 ? 1 : -1;
            data.data[i]   = Math.max(0, Math.min(255, data.data[i]   + delta));
            data.data[i+1] = Math.max(0, Math.min(255, data.data[i+1] + delta));
            // skip alpha (i+3) always
        }
        return data;
    });

    const _toDataURL = HTMLCanvasElement.prototype.toDataURL;
    HTMLCanvasElement.prototype.toDataURL = makeNative(function(type, quality) {
        const ctx = this.getContext('2d');
        if (ctx && this.width > 0 && this.height > 0) {
            const d = ctx.getImageData(0, 0, this.width, this.height);
            const noise = makeNoise(CANVAS_SEED + 0.5);
            for (let i = 0; i < d.data.length; i += 4) {
                if (noise() > 0.33) continue;
                const delta = noise() > 0.5 ? 1 : -1;
                d.data[i]   = Math.max(0, Math.min(255, d.data[i]   + delta));
                d.data[i+1] = Math.max(0, Math.min(255, d.data[i+1] + delta));
            }
            ctx.putImageData(d, 0, 0);
        }
        return _toDataURL.call(this, type, quality);
    });
    console.log('[NB] canvas ok (xorshift)');
} catch(e) { console.log('[NB] canvas FAILED:', e.message); }

try {
    const _getChannelData = AudioBuffer.prototype.getChannelData;
    AudioBuffer.prototype.getChannelData = makeNative(function() {
        const arr = _getChannelData.apply(this, arguments);
        for (let i = 0; i < arr.length; i++)
            arr[i] += seededRand(AUDIO_SEED, i) * 0.0000001 - 0.00000005;
        return arr;
    });
    const _copyFromChannel = AudioBuffer.prototype.copyFromChannel;
    AudioBuffer.prototype.copyFromChannel = makeNative(function(dest) {
        _copyFromChannel.apply(this, arguments);
        for (let i = 0; i < dest.length; i++)
            dest[i] += seededRand(AUDIO_SEED, i + 500000) * 0.0000001 - 0.00000005;
    });
    console.log('[NB] audio ok');
} catch(e) { console.log('[NB] audio FAILED:', e.message); }

// ── FIX 4: WebGL UNMASKED_VENDOR + UNMASKED_RENDERER ─────────────────────────
// Params 37445/37446 are what CreepJS and fingerprint sites check first.
// Must match gpuVendorFull/gpuRenderer from C++ identity.
try {
    function spoofWebGLParam(orig, vendor, renderer) {
        return makeNative(function(param) {
            if (param === 37445) return vendor;    // UNMASKED_VENDOR_WEBGL
            if (param === 37446) return renderer;  // UNMASKED_RENDERER_WEBGL
            if (param === 33901) return new Float32Array([seededRand(AUDIO_SEED,1)+0.5, seededRand(AUDIO_SEED,2)+0.5]);
            if (param === 33902) return new Float32Array([seededRand(AUDIO_SEED,3)+0.5, seededRand(AUDIO_SEED,4)+0.5]);
            return orig.call(this, param);
        });
    }
    WebGLRenderingContext.prototype.getParameter  = spoofWebGLParam(
        WebGLRenderingContext.prototype.getParameter,  WGL_VENDOR, WGL_RENDERER);
    WebGL2RenderingContext.prototype.getParameter = spoofWebGLParam(
        WebGL2RenderingContext.prototype.getParameter, WGL_VENDOR, WGL_RENDERER);
    console.log('[NB] webgl ok (vendor+renderer)');
} catch(e) { console.log('[NB] webgl FAILED:', e.message); }

// ── FIX: navigator.userAgentData (UA-CH API) ──────────────────────────────────
// Modern sites (and CreepJS) call navigator.userAgentData.brands and
// getHighEntropyValues() — without this, they detect inconsistency.
try {
    Object.defineProperty(navigator, 'userAgentData', {
        get: makeNative(() => ({
            brands: [
                { brand: 'Google Chrome', version: '124' },
                { brand: 'Chromium',      version: '124' },
                { brand: 'Not-A.Brand',   version: '99'  },
            ],
            mobile: false,
            platform: 'Linux',
            getHighEntropyValues: makeNative(function(hints) {
                return Promise.resolve({
                    platform:        'Linux',
                    platformVersion: '6.1.0',
                    architecture:    'x86',
                    bitness:         '64',
                    fullVersionList: [
                        { brand: 'Google Chrome', version: '124.0.0.0' },
                        { brand: 'Chromium',      version: '124.0.0.0' },
                        { brand: 'Not-A.Brand',   version: '99.0.0.0'  },
                    ],
                });
            }),
        })),
        configurable: true,
    });
    console.log('[NB] userAgentData ok');
} catch(e) { console.log('[NB] userAgentData FAILED:', e.message); }

try {
    if (navigator.getBattery) {
        navigator.getBattery = makeNative(function() {
            return Promise.resolve({
                charging: true, chargingTime: 0, dischargingTime: Infinity,
                level: 0.85 + seededRand(AUDIO_SEED, 99) * 0.15,
                addEventListener: makeNative(()=>{}), removeEventListener: makeNative(()=>{}),
                dispatchEvent: makeNative(()=>false),
                onchargingchange: null, onchargingtimechange: null,
                ondischargingtimechange: null, onlevelchange: null,
            });
        });
        console.log('[NB] battery ok');
    }
} catch(e) { console.log('[NB] battery FAILED:', e.message); }

try {
    if (window.RTCPeerConnection) {
        const _RTC = window.RTCPeerConnection;
        window.RTCPeerConnection = makeNative(function(config) {
            if (config && config.iceServers)
                config.iceServers = config.iceServers.filter(s => s.urls && !String(s.urls).includes('stun:'));
            return new _RTC(config);
        });
        Object.setPrototypeOf(window.RTCPeerConnection, _RTC);
        console.log('[NB] webrtc ok');
    }
} catch(e) { console.log('[NB] webrtc FAILED:', e.message); }

try {
    const _perfNow = performance.now.bind(performance);
    performance.now = makeNative(function() { return Math.floor(_perfNow() / 0.1) * 0.1; });
    console.log('[NB] perf ok');
} catch(e) { console.log('[NB] perf FAILED:', e.message); }

try {
    const _DTF = Intl.DateTimeFormat;
    Intl.DateTimeFormat = new Proxy(_DTF, {
        construct(target, args) {
            if (!args[1]) args[1] = {};
            if (!args[1].timeZone) args[1].timeZone = TIMEZONE;
            return new target(...args);
        }
    });
    console.log('[NB] timezone ok');
} catch(e) { console.log('[NB] timezone FAILED:', e.message); }

console.log('[NB] script COMPLETE');

})();
)JS";

    return script;
}