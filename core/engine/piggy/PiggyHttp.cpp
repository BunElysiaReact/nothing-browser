// ─── PiggyHttp.cpp — merged into PiggyServer.cpp ──────────────────────────────
//
// The separate raw-HTTP JSON API (QTcpServer on port 2005) has been folded
// into the single WebSocket server in PiggyServer.cpp. There is now exactly
// one listener, exactly one port (2005, fixed), and exactly one protocol
// (WebSocket text frames carrying the same {id, cmd, payload} JSON envelope
// as before) for both local and remote scripts.
//
// This file is kept as an empty translation unit on purpose, in case your
// build file still lists it as a source — nothing to update there. Feel
// free to delete it and remove it from the build once you've confirmed
// everything else compiles.
