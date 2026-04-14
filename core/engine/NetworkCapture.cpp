#include "NetworkCapture.h"
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

NetworkCapture::NetworkCapture(QObject *parent) : QObject(parent) {}

void NetworkCapture::attachToPage(QWebEnginePage *page, QWebEngineProfile *profile) {
    m_page    = page;
    m_profile = profile;

    auto *store = profile->cookieStore();
    store->loadAllCookies();
    connect(store, &QWebEngineCookieStore::cookieAdded,
            this,  &NetworkCapture::onCookieAdded);
    connect(store, &QWebEngineCookieStore::cookieRemoved,
            this,  &NetworkCapture::onCookieRemoved);

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        if (!m_page) return;
        m_page->runJavaScript(
            "(function(){"
            "  var q=window.__NOTHING_QUEUE__;"
            "  if(!q||q.length===0) return '[]';"
            "  var out=JSON.stringify(q);"
            "  window.__NOTHING_QUEUE__=[];"
            "  return out;"
            "})()",
            [this](const QVariant &result) {
                QString json = result.toString();
                if (json.isEmpty() || json == "[]") return;
                onJsMessage(json);
            }
        );
    });
    timer->start(250);
}

void NetworkCapture::onJsMessage(const QString &json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return;

    for (const auto &val : doc.array()) {
        QJsonObject obj = val.toObject();
        QString type = obj["type"].toString();

        if (type == "request" || type == "response") {
            CapturedRequest req;
            req.id              = obj["id"].toString();
            req.method          = obj["method"].toString();
            req.url             = obj["url"].toString();
            req.type            = obj["reqType"].toString();
            req.status          = obj["status"].toString();
            req.mimeType        = obj["mime"].toString();
            req.requestHeaders  = obj["reqHeaders"].toString();
            req.requestBody     = obj["reqBody"].toString();   // ← fixed
            req.responseHeaders = obj["resHeaders"].toString();
            req.responseBody    = obj["body"].toString();
            req.size            = obj["size"].toInt();
            req.timestamp       = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
            emit requestCaptured(req);

        } else if (type == "ws_open") {
            WebSocketFrame f;
            f.connectionId = obj["id"].toString();
            f.url          = obj["url"].toString();
            f.direction    = "OPEN";
            f.data         = "[WebSocket opened]";
            f.timestamp    = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
            emit wsFrameCaptured(f);

        } else if (type == "ws_send" || type == "ws_recv") {
            WebSocketFrame f;
            f.connectionId = obj["id"].toString();
            f.url          = obj["url"].toString();
            f.direction    = (type == "ws_send") ? "UP SENT" : "DN RECV";
            f.data         = obj["data"].toString();
            f.isBinary     = obj["binary"].toBool();
            f.timestamp    = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
            emit wsFrameCaptured(f);

        } else if (type == "ws_close") {
            WebSocketFrame f;
            f.connectionId = obj["id"].toString();
            f.url          = obj["url"].toString();
            f.direction    = "CLOSED";
            f.data         = QString("[Closed] code=%1 reason=%2")
                               .arg(obj["code"].toInt())
                               .arg(obj["reason"].toString());
            f.timestamp    = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
            emit wsFrameCaptured(f);

        } else if (type == "storage") {
            emit storageCaptured(
                obj["origin"].toString(),
                obj["key"].toString(),
                obj["value"].toString(),
                obj["storageType"].toString()
            );

        } else if (type == "exposed_call") {
            emit exposedFunctionCalled(
                obj["name"].toString(),
                obj["callId"].toString(),
                obj["data"].toString()
            );
        }
    }
}

void NetworkCapture::onCookieAdded(const QNetworkCookie &c) {
    CapturedCookie cookie;
    cookie.name     = QString::fromUtf8(c.name());
    cookie.value    = QString::fromUtf8(c.value());
    cookie.domain   = c.domain();
    cookie.path     = c.path();
    cookie.httpOnly = c.isHttpOnly();
    cookie.secure   = c.isSecure();
    if (!c.expirationDate().isNull())
        cookie.expires = c.expirationDate().toString(Qt::ISODate);
    emit cookieCaptured(cookie);
}

void NetworkCapture::onCookieRemoved(const QNetworkCookie &c) {
    emit cookieRemoved(QString::fromUtf8(c.name()), c.domain());
}

QString NetworkCapture::exposeFunctionScript(const QString &name) {
    return QString(R"JS(
(function() {
    'use strict';
    var _name = '%1';
    
    if (!window.__NOTHING_EXPOSED_RESOLVERS__) {
        window.__NOTHING_EXPOSED_RESOLVERS__ = {};
    }
    
    window[_name] = function(data) {
        return new Promise(function(resolve, reject) {
            var callId = Math.random().toString(36).slice(2) + Date.now().toString(36);
            
            window.__NOTHING_EXPOSED_RESOLVERS__[callId] = { 
                resolve: resolve, 
                reject: reject 
            };
            
            var payload = typeof data === 'string' ? data : JSON.stringify(data);
            window.__NOTHING_QUEUE__.push({
                type:   'exposed_call',
                name:   _name,
                callId: callId,
                data:   payload
            });
        });
    };

    window.__NOTHING_RESOLVE_EXPOSED__ = function(callId, result, isError) {
        var r = window.__NOTHING_EXPOSED_RESOLVERS__ && 
                window.__NOTHING_EXPOSED_RESOLVERS__[callId];
        if (!r) return;
        delete window.__NOTHING_EXPOSED_RESOLVERS__[callId];
        if (isError) {
            r.reject(new Error(result));
        } else {
            try {
                r.resolve(JSON.parse(result));
            } catch(e) {
                r.resolve(result);
            }
        }
    };
})();
)JS").arg(name);
}

QString NetworkCapture::captureScript() {
    return R"JS(
(function() {
'use strict';
if (window.__NOTHING_CAPTURE_INIT__) return;
window.__NOTHING_CAPTURE_INIT__ = true;
window.__NOTHING_QUEUE__ = [];

var push = function(obj) { window.__NOTHING_QUEUE__.push(obj); };
var uid  = function() { return Math.random().toString(36).slice(2,10)+Date.now().toString(36); };

function bufToB64(buf) {
    var bytes = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
    var bin = '';
    for (var i = 0; i < bytes.byteLength; i++) bin += String.fromCharCode(bytes[i]);
    return btoa(bin);
}

// Normalize request body to a string regardless of what was passed
function extractBody(body) {
    if (!body) return '';
    if (typeof body === 'string') return body.slice(0, 8000);
    if (body instanceof URLSearchParams) return body.toString().slice(0, 8000);
    if (body instanceof FormData) {
        var out = '';
        try { body.forEach(function(v,k){ out += k+'='+v+'&'; }); } catch(e) {}
        return out.slice(0, 8000);
    }
    if (body instanceof ArrayBuffer || ArrayBuffer.isView(body)) {
        try { return bufToB64(body); } catch(e) { return '[binary body]'; }
    }
    try { return String(body).slice(0, 8000); } catch(e) { return ''; }
}

// ── fetch ─────────────────────────────────────────────────────────────────────
var _fetch = window.fetch;
window.fetch = function(input, init) {
    var id      = uid();
    var url     = typeof input === 'string' ? input : (input.url || String(input));
    var method  = (init && init.method) ? init.method.toUpperCase() : 'GET';
    var reqBody = (init && init.body) ? extractBody(init.body) : '';
    var reqHdrs = '';
    try {
        var h = (init && init.headers) ? init.headers : {};
        if (h instanceof Headers) {
            var tmp = {};
            h.forEach(function(v,k){ tmp[k]=v; });
            h = tmp;
        }
        reqHdrs = JSON.stringify(h);
    } catch(e) {}

    push({ type:'request', id:id, method:method, url:url,
           reqType:'Fetch', reqHeaders:reqHdrs, reqBody:reqBody });

    return _fetch.apply(this, arguments).then(function(resp) {
        var status = resp.status;
        var mime   = resp.headers.get('content-type') || '';
        resp.clone().text().then(function(body) {
            push({ type:'response', id:id, method:method, url:url, reqType:'Fetch',
                   status:String(status), mime:mime, body:body.slice(0,8000),
                   size:body.length, reqHeaders:reqHdrs, reqBody:reqBody, resHeaders:'' });
        }).catch(function(){});
        return resp;
    });
};

// ── XHR ───────────────────────────────────────────────────────────────────────
var _open   = XMLHttpRequest.prototype.open;
var _send   = XMLHttpRequest.prototype.send;
var _setHdr = XMLHttpRequest.prototype.setRequestHeader;

XMLHttpRequest.prototype.open = function(method, url) {
    this.__n_id__     = uid();
    this.__n_method__ = method.toUpperCase();
    this.__n_url__    = url;
    this.__n_hdrs__   = {};
    return _open.apply(this, arguments);
};
XMLHttpRequest.prototype.setRequestHeader = function(k, v) {
    if (this.__n_hdrs__) this.__n_hdrs__[k] = v;
    return _setHdr.apply(this, arguments);
};
XMLHttpRequest.prototype.send = function(body) {
    var self    = this;
    var id      = self.__n_id__;
    var method  = self.__n_method__ || 'GET';
    var url     = self.__n_url__    || '';
    var reqBody = extractBody(body);
    var reqHdrs = '';
    try { reqHdrs = JSON.stringify(self.__n_hdrs__ || {}); } catch(e) {}

    push({ type:'request', id:id, method:method, url:url,
           reqType:'XHR', reqHeaders:reqHdrs, reqBody:reqBody });

    self.addEventListener('load', function() {
        var text = ''; try { text = self.responseText || ''; } catch(e) {}
        push({ type:'response', id:id, method:method, url:url, reqType:'XHR',
               status:String(self.status),
               mime:self.getResponseHeader('content-type') || '',
               body:text.slice(0,8000), size:text.length,
               reqHeaders:reqHdrs, reqBody:reqBody, resHeaders:'' });
    });
    return _send.apply(this, arguments);
};

// ── WebSocket — full binary capture ───────────────────────────────────────────
var _WS = window.WebSocket;
window.WebSocket = function(url, protocols) {
    var id = uid();
    var ws = protocols ? new _WS(url, protocols) : new _WS(url);
    ws.binaryType = 'arraybuffer';
    push({ type:'ws_open', id:id, url:url });

    var _wsSend = ws.send.bind(ws);
    ws.send = function(data) {
        if (data instanceof ArrayBuffer)
            push({ type:'ws_send', id:id, url:url, data:bufToB64(data), binary:true });
        else if (data instanceof Blob) {
            var fr = new FileReader();
            fr.onload = function() {
                push({ type:'ws_send', id:id, url:url, data:bufToB64(fr.result), binary:true });
            };
            fr.readAsArrayBuffer(data);
        } else {
            push({ type:'ws_send', id:id, url:url,
                   data:String(data).slice(0,8000), binary:false });
        }
        return _wsSend(data);
    };

    ws.addEventListener('message', function(e) {
        if (e.data instanceof ArrayBuffer)
            push({ type:'ws_recv', id:id, url:url, data:bufToB64(e.data), binary:true });
        else if (e.data instanceof Blob) {
            var fr = new FileReader();
            fr.onload = function() {
                push({ type:'ws_recv', id:id, url:url, data:bufToB64(fr.result), binary:true });
            };
            fr.readAsArrayBuffer(e.data);
        } else {
            push({ type:'ws_recv', id:id, url:url,
                   data:String(e.data).slice(0,8000), binary:false });
        }
    });

    ws.addEventListener('close', function(e) {
        push({ type:'ws_close', id:id, url:url, code:e.code, reason:e.reason });
    });

    return ws;
};
window.WebSocket.prototype  = _WS.prototype;
window.WebSocket.CONNECTING = _WS.CONNECTING;
window.WebSocket.OPEN       = _WS.OPEN;
window.WebSocket.CLOSING    = _WS.CLOSING;
window.WebSocket.CLOSED     = _WS.CLOSED;

// ── localStorage / sessionStorage ─────────────────────────────────────────────
function wrapStorage(store, label) {
    var _si = store.setItem.bind(store);
    store.setItem = function(key, value) {
        push({ type:'storage', storageType:label,
               origin:window.location.origin,
               key:key, value:String(value).slice(0,2000) });
        return _si(key, value);
    };
}
try { wrapStorage(window.localStorage,   'localStorage');   } catch(e) {}
try { wrapStorage(window.sessionStorage, 'sessionStorage'); } catch(e) {}

})();
)JS";
}

QString NetworkCapture::workerCaptureScript() {
    return R"JS(
// Worker capture - placeholder for now
console.log('[Nothing] Worker capture initialized');
)JS";
}