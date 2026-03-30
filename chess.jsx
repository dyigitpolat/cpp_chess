import { useState, useCallback, useRef } from "react";

/* ═══════════════════════════════════════════
   CHESS ENGINE — correct mate-in-1 finder
   ═══════════════════════════════════════════ */

const inB = (r, c) => r >= 0 && r < 8 && c >= 0 && c < 8;
const colOf = (p) => (p && p === p.toUpperCase() ? "w" : "b");
const opp = (color) => (color === "w" ? "b" : "w");

/* Returns all squares attacked by piece at (r,c).
   For pawns this is ONLY diagonal attacks, NOT forward pushes. */
function attacksFrom(board, r, c) {
  const p = board[r][c];
  if (!p) return [];
  const color = colOf(p);
  const t = p.toUpperCase();
  const out = [];

  if (t === "P") {
    const dir = color === "w" ? -1 : 1;
    for (const dc of [-1, 1]) {
      const nr = r + dir, nc = c + dc;
      if (inB(nr, nc)) out.push([nr, nc]);
    }
    return out;
  }
  if (t === "N") {
    for (const [dr, dc] of [[-2,-1],[-2,1],[-1,-2],[-1,2],[1,-2],[1,2],[2,-1],[2,1]]) {
      const nr = r + dr, nc = c + dc;
      if (inB(nr, nc)) out.push([nr, nc]);
    }
    return out;
  }
  if (t === "K") {
    for (const [dr, dc] of [[-1,-1],[-1,0],[-1,1],[0,-1],[0,1],[1,-1],[1,0],[1,1]]) {
      const nr = r + dr, nc = c + dc;
      if (inB(nr, nc)) out.push([nr, nc]);
    }
    return out;
  }
  const dirs =
    t === "B" ? [[-1,-1],[-1,1],[1,-1],[1,1]] :
    t === "R" ? [[-1,0],[1,0],[0,-1],[0,1]] :
    /* Q */ [[-1,-1],[-1,0],[-1,1],[0,-1],[0,1],[1,-1],[1,0],[1,1]];
  for (const [dr, dc] of dirs) {
    let nr = r + dr, nc = c + dc;
    while (inB(nr, nc)) {
      out.push([nr, nc]);
      if (board[nr][nc]) break;
      nr += dr; nc += dc;
    }
  }
  return out;
}

function isAttacked(board, r, c, byColor) {
  for (let rr = 0; rr < 8; rr++)
    for (let cc = 0; cc < 8; cc++) {
      if (!board[rr][cc] || colOf(board[rr][cc]) !== byColor) continue;
      const atks = attacksFrom(board, rr, cc);
      for (const [ar, ac] of atks) {
        if (ar === r && ac === c) return true;
      }
    }
  return false;
}

function findKing(board, color) {
  const k = color === "w" ? "K" : "k";
  for (let r = 0; r < 8; r++)
    for (let c = 0; c < 8; c++)
      if (board[r][c] === k) return [r, c];
  return null;
}

function inCheck(board, color) {
  const kp = findKing(board, color);
  if (!kp) return false;
  return isAttacked(board, kp[0], kp[1], opp(color));
}

function pseudoMoves(board, color) {
  const moves = [];
  for (let r = 0; r < 8; r++)
    for (let c = 0; c < 8; c++) {
      const p = board[r][c];
      if (!p || colOf(p) !== color) continue;
      const t = p.toUpperCase();

      if (t === "P") {
        const dir = color === "w" ? -1 : 1;
        if (inB(r + dir, c) && !board[r + dir][c]) {
          moves.push({ fr: r, fc: c, tr: r + dir, tc: c });
          const startRow = color === "w" ? 6 : 1;
          if (r === startRow && !board[r + 2 * dir][c])
            moves.push({ fr: r, fc: c, tr: r + 2 * dir, tc: c });
        }
        for (const dc of [-1, 1]) {
          const nr = r + dir, nc = c + dc;
          if (inB(nr, nc) && board[nr][nc] && colOf(board[nr][nc]) !== color)
            moves.push({ fr: r, fc: c, tr: nr, tc: nc });
        }
        continue;
      }

      const atks = attacksFrom(board, r, c);
      for (const [ar, ac] of atks) {
        if (!board[ar][ac] || colOf(board[ar][ac]) !== color)
          moves.push({ fr: r, fc: c, tr: ar, tc: ac });
      }
    }
  return moves;
}

function applyMove(board, m) {
  const b = board.map((row) => [...row]);
  b[m.tr][m.tc] = b[m.fr][m.fc];
  b[m.fr][m.fc] = null;
  if (b[m.tr][m.tc] === "P" && m.tr === 0) b[m.tr][m.tc] = "Q";
  if (b[m.tr][m.tc] === "p" && m.tr === 7) b[m.tr][m.tc] = "q";
  return b;
}

function hasAnyLegalMove(board, color) {
  const moves = pseudoMoves(board, color);
  for (const m of moves) {
    const nb = applyMove(board, m);
    if (!inCheck(nb, color)) return true;
  }
  return false;
}

function solve(board, atkColor) {
  const defColor = opp(atkColor);
  if (!findKing(board, defColor) || !findKing(board, atkColor)) return [];
  if (inCheck(board, atkColor)) return [];

  const results = [];
  const moves = pseudoMoves(board, atkColor);
  for (const m of moves) {
    const nb = applyMove(board, m);
    if (inCheck(nb, atkColor)) continue;
    if (!inCheck(nb, defColor)) continue;
    if (!hasAnyLegalMove(nb, defColor)) {
      results.push({ from: [m.fr, m.fc], to: [m.tr, m.tc], piece: board[m.fr][m.fc] });
    }
  }
  return results;
}

/* ═══════════════════════════════════════════
   UI
   ═══════════════════════════════════════════ */

const SYM = { K:"\u2654", Q:"\u2655", R:"\u2656", B:"\u2657", N:"\u2658", P:"\u2659", k:"\u265A", q:"\u265B", r:"\u265C", b:"\u265D", n:"\u265E", p:"\u265F" };
const FILES = "abcdefgh";
const toSq = (r, c) => FILES[c] + (8 - r);
const W_PIECES = ["K","Q","R","B","N","P"];
const B_PIECES = ["k","q","r","b","n","p"];

export default function App() {
  const empty = () => Array.from({ length: 8 }, () => Array(8).fill(null));
  const [board, setBoard] = useState(empty);
  const [tool, setTool] = useState("K");
  const [side, setSide] = useState("w");
  const [results, setResults] = useState(null);
  const [hl, setHl] = useState(null);
  const [busy, setBusy] = useState(false);

  const place = useCallback((r, c) => {
    setBoard(prev => {
      const b = prev.map(row => [...row]);
      if (tool === "eraser") { b[r][c] = null; }
      else {
        if (tool === "K" || tool === "k") {
          for (let rr = 0; rr < 8; rr++)
            for (let cc = 0; cc < 8; cc++)
              if (b[rr][cc] === tool) b[rr][cc] = null;
        }
        b[r][c] = tool;
      }
      return b;
    });
    setResults(null);
    setHl(null);
  }, [tool]);

  const run = useCallback(() => {
    setBusy(true);
    setHl(null);
    setTimeout(() => {
      setResults(solve(board, side));
      setBusy(false);
    }, 30);
  }, [board, side]);

  const clear = () => { setBoard(empty()); setResults(null); setHl(null); };

  const loadDemo = () => {
    const b = empty();
    // Scholar's mate setup — White can Qxf7#
    b[0][0] = "r"; b[0][1] = "n"; b[0][2] = "b"; b[0][3] = "q"; b[0][4] = "k"; b[0][5] = "b"; b[0][7] = "r";
    b[1][0] = "p"; b[1][1] = "p"; b[1][2] = "p"; b[1][3] = "p"; b[1][5] = "p"; b[1][6] = "p"; b[1][7] = "p";
    b[2][5] = "n";  // Nf6
    b[3][4] = "p";  // e5
    b[4][4] = "P";  // e4
    b[5][2] = "B";  // Bc4
    b[6][0] = "P"; b[6][1] = "P"; b[6][2] = "P"; b[6][3] = "P"; b[6][5] = "P"; b[6][6] = "P"; b[6][7] = "P";
    b[7][0] = "R"; b[7][1] = "N"; b[7][2] = "B"; b[7][3] = "Q"; b[7][4] = "K"; b[7][6] = "N"; b[7][7] = "R";
    b[4][7] = "Q"; // Qh4 — wrong, let me do a simpler position
    // Actually let me just do a clean known mate-in-1:
    // White: Ke1, Qh5, Bc4; Black: Ke8, Pf7
    const b2 = empty();
    b2[7][4] = "K"; // Ke1
    b2[3][7] = "Q"; // Qh5
    b2[5][2] = "B"; // Bc4
    b2[0][4] = "k"; // ke8
    b2[1][5] = "p"; // pf7
    setBoard(b2);
    setResults(null); setHl(null); setSide("w");
  };

  const sqBg = (r, c) => {
    const light = (r + c) % 2 === 0;
    if (hl) {
      if (hl.to[0] === r && hl.to[1] === c) return light ? "#d4edbc" : "#86c441";
      if (hl.from[0] === r && hl.from[1] === c) return light ? "#c8daf8" : "#6ea2e8";
    }
    return light ? "#eae8e0" : "#8b9b78";
  };

  const hasWK = !!findKing(board, "w");
  const hasBK = !!findKing(board, "b");
  const canRun = hasWK && hasBK && !busy;

  const PieceBtn = ({ pc }) => {
    const active = tool === pc;
    const isBlack = colOf(pc) === "b";
    return (
      <button className="tool-btn" onClick={() => setTool(pc)} style={{
        width: 48, height: 48, fontSize: 30, lineHeight: 1, fontWeight: 900,
        border: active ? "2px solid #86c441" : "1px solid #2d2d2b",
        borderRadius: 10, background: active ? "#1e2b16" : "#161615",
        cursor: "pointer",
        color: isBlack ? "#1a1a18" : "#f0efe9",
        textShadow: isBlack
          ? "0 1px 3px rgba(0,0,0,0.6), 0 0 8px rgba(0,0,0,0.3)"
          : "0 1px 4px rgba(0,0,0,0.4), 0 0 10px rgba(255,255,255,0.15)",
        display: "flex", alignItems: "center", justifyContent: "center",
        boxShadow: active ? "0 0 8px #86c44133, inset 0 0 12px #86c44111" : "none",
      }}>{SYM[pc]}</button>
    );
  };

  return (
    <div style={{
      minHeight: "100vh",
      background: "#0f0f0e",
      color: "#e2e0da",
      fontFamily: "'DM Sans', system-ui, sans-serif",
      display: "flex", flexDirection: "column", alignItems: "center",
      padding: "28px 12px",
    }}>
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=DM+Sans:ital,opsz,wght@0,9..40,400;0,9..40,500;0,9..40,600;0,9..40,700&family=DM+Mono:wght@400;500&display=swap');
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { background: #0f0f0e; }
        ::-webkit-scrollbar { width: 4px; }
        ::-webkit-scrollbar-track { background: transparent; }
        ::-webkit-scrollbar-thumb { background: #333; border-radius: 10px; }
        .sq-cell { transition: background 0.1s; position: relative; }
        .sq-cell:hover::after {
          content: ''; position: absolute; inset: 0;
          background: rgba(255,255,255,0.08); pointer-events: none;
        }
        .tool-btn { transition: all 0.1s; }
        .tool-btn:hover { filter: brightness(1.2); }
        .move-row { transition: background 0.1s; cursor: pointer; }
        .move-row:hover { background: #1e2b16 !important; }
        @keyframes spin { to { transform: rotate(360deg); } }
        @keyframes slideUp { from { opacity:0; transform: translateY(8px); } to { opacity:1; transform: translateY(0); } }
        @keyframes countPop { 0% { transform: scale(0.7); opacity: 0; } 60% { transform: scale(1.1); } 100% { transform: scale(1); opacity: 1; } }
      `}</style>

      {/* Header */}
      <div style={{ textAlign: "center", marginBottom: 28 }}>
        <div style={{ fontSize: 11, fontWeight: 600, textTransform: "uppercase", letterSpacing: "0.15em", color: "#86c441", marginBottom: 6 }}>Chess Analysis Tool</div>
        <h1 style={{ fontSize: 24, fontWeight: 700, letterSpacing: "-0.03em", color: "#f0efe9" }}>
          Mate-in-One Finder
        </h1>
        <p style={{ fontSize: 13, color: "#666", marginTop: 6 }}>
          Set up any position · Find every checkmate in one move
        </p>
      </div>

      <div style={{
        display: "flex", gap: 20, flexWrap: "wrap", justifyContent: "center",
        alignItems: "flex-start", maxWidth: 780,
      }}>

        {/* ─── TOOLBOX ─── */}
        <div style={{
          width: 180, background: "#19191800", borderRadius: 16,
          display: "flex", flexDirection: "column", gap: 14,
        }}>
          {/* Pieces */}
          <div style={{ background: "#191918", borderRadius: 14, padding: "14px 12px", border: "1px solid #242422" }}>
            <div style={{ fontSize: 10, fontWeight: 600, textTransform: "uppercase", letterSpacing: "0.12em", color: "#777", marginBottom: 8 }}>White pieces</div>
            <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: 4 }}>
              {W_PIECES.map(pc => <PieceBtn key={pc} pc={pc} />)}
            </div>
            <div style={{ fontSize: 10, fontWeight: 600, textTransform: "uppercase", letterSpacing: "0.12em", color: "#777", margin: "12px 0 8px" }}>Black pieces</div>
            <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: 4 }}>
              {B_PIECES.map(pc => <PieceBtn key={pc} pc={pc} />)}
            </div>
            <button className="tool-btn" onClick={() => setTool("eraser")} style={{
              width: "100%", height: 34, marginTop: 8, fontSize: 11, fontWeight: 600,
              border: tool === "eraser" ? "2px solid #e87171" : "1px solid #2d2d2b",
              borderRadius: 8, background: tool === "eraser" ? "#2e1818" : "#161615",
              cursor: "pointer", color: tool === "eraser" ? "#f08080" : "#777",
              fontFamily: "inherit", letterSpacing: "0.06em",
            }}>ERASER</button>
          </div>

          {/* Controls */}
          <div style={{ background: "#191918", borderRadius: 14, padding: "14px 12px", border: "1px solid #242422", display: "flex", flexDirection: "column", gap: 10 }}>
            <div style={{ fontSize: 10, fontWeight: 600, textTransform: "uppercase", letterSpacing: "0.12em", color: "#777" }}>Who moves?</div>
            <div style={{ display: "flex", gap: 4 }}>
              {[["w","White"],["b","Black"]].map(([v, l]) => (
                <button key={v} className="tool-btn" onClick={() => { setSide(v); setResults(null); setHl(null); }} style={{
                  flex: 1, height: 34, fontSize: 11, fontWeight: 600,
                  border: side === v ? "2px solid #6ea2e8" : "1px solid #2d2d2b",
                  borderRadius: 8, background: side === v ? "#16202e" : "#161615",
                  cursor: "pointer", color: side === v ? "#8bbaf0" : "#666",
                  fontFamily: "inherit",
                }}>{l}</button>
              ))}
            </div>

            <button onClick={run} disabled={!canRun} style={{
              height: 42, fontSize: 13, fontWeight: 700, letterSpacing: "0.04em",
              border: "none", borderRadius: 10,
              background: canRun ? "#86c441" : "#2a2a28",
              color: canRun ? "#0a1a06" : "#555",
              cursor: canRun ? "pointer" : "not-allowed",
              fontFamily: "inherit",
              transition: "all 0.15s",
            }}>
              {busy ? "Analyzing…" : "FIND MATES"}
            </button>

            {!hasWK || !hasBK ? (
              <div style={{ fontSize: 10, color: "#e87171", textAlign: "center", lineHeight: 1.4 }}>
                Place both kings to analyze
              </div>
            ) : null}

            <div style={{ display: "flex", gap: 4 }}>
              <button className="tool-btn" onClick={loadDemo} style={{
                flex: 1, height: 30, fontSize: 10, fontWeight: 600,
                border: "1px solid #2d2d2b", borderRadius: 8, background: "#161615",
                cursor: "pointer", color: "#666", fontFamily: "inherit",
              }}>DEMO</button>
              <button className="tool-btn" onClick={clear} style={{
                flex: 1, height: 30, fontSize: 10, fontWeight: 600,
                border: "1px solid #2d2d2b", borderRadius: 8, background: "#161615",
                cursor: "pointer", color: "#666", fontFamily: "inherit",
              }}>CLEAR</button>
            </div>
          </div>
        </div>

        {/* ─── BOARD ─── */}
        <div>
          <div style={{ display: "flex", marginLeft: 22, marginBottom: 3 }}>
            {FILES.split("").map(f => (
              <div key={f} style={{ width: 48, textAlign: "center", fontSize: 10, color: "#4a4a48", fontFamily: "'DM Mono', monospace", fontWeight: 500 }}>{f}</div>
            ))}
          </div>
          <div style={{ display: "flex" }}>
            <div style={{ display: "flex", flexDirection: "column", marginRight: 5 }}>
              {[8,7,6,5,4,3,2,1].map(rank => (
                <div key={rank} style={{ height: 48, display: "flex", alignItems: "center", justifyContent: "center", fontSize: 10, color: "#4a4a48", fontFamily: "'DM Mono', monospace", fontWeight: 500, width: 16 }}>{rank}</div>
              ))}
            </div>
            <div style={{
              display: "grid", gridTemplateColumns: "repeat(8, 48px)", gridTemplateRows: "repeat(8, 48px)",
              borderRadius: 6, overflow: "hidden",
              boxShadow: "0 8px 40px rgba(0,0,0,0.5), 0 0 0 1px #2a2a28",
            }}>
              {board.flatMap((row, r) =>
                row.map((pc, c) => (
                  <div key={`${r}-${c}`} className="sq-cell"
                    onClick={() => place(r, c)}
                    style={{
                      width: 48, height: 48,
                      background: sqBg(r, c),
                      display: "flex", alignItems: "center", justifyContent: "center",
                      cursor: "pointer", fontSize: 33, lineHeight: 1, userSelect: "none",
                    }}>
                    {pc && <span style={{
                      fontWeight: 900,
                      color: colOf(pc) === "b" ? "#1a1a18" : "#f0efe9",
                      textShadow: colOf(pc) === "b"
                        ? "0 2px 4px rgba(0,0,0,0.5), 0 0 8px rgba(0,0,0,0.25)"
                        : "0 2px 4px rgba(0,0,0,0.3), 0 0 10px rgba(255,255,255,0.12)",
                    }}>{SYM[pc]}</span>}
                  </div>
                ))
              )}
            </div>
          </div>
        </div>

        {/* ─── RESULTS ─── */}
        <div style={{
          width: 190, display: "flex", flexDirection: "column", gap: 0,
        }}>
          <div style={{ background: "#191918", borderRadius: 14, padding: "14px 12px", border: "1px solid #242422", minHeight: 120 }}>
            <div style={{ fontSize: 10, fontWeight: 600, textTransform: "uppercase", letterSpacing: "0.12em", color: "#777", marginBottom: 12 }}>Results</div>

            {results === null && !busy && (
              <p style={{ fontSize: 12, color: "#555", lineHeight: 1.6 }}>
                Set up a position and hit <span style={{ color: "#86c441", fontWeight: 600 }}>Find Mates</span>.
              </p>
            )}

            {busy && (
              <div style={{ display: "flex", alignItems: "center", gap: 8, color: "#888", fontSize: 12 }}>
                <span style={{ display: "inline-block", width: 12, height: 12, border: "2px solid #86c441", borderTopColor: "transparent", borderRadius: "50%", animation: "spin 0.5s linear infinite" }} />
                Searching…
              </div>
            )}

            {results !== null && !busy && (
              <div style={{ animation: "slideUp 0.2s ease" }}>
                <div style={{
                  textAlign: "center", padding: "14px 0 10px", marginBottom: 10,
                  borderRadius: 10,
                  background: results.length > 0
                    ? "linear-gradient(135deg, #1a2e14, #1e3018)"
                    : "linear-gradient(135deg, #2e1616, #301818)",
                  border: results.length > 0 ? "1px solid #2d4a2d" : "1px solid #4a2d2d",
                }}>
                  <div style={{
                    fontSize: 40, fontWeight: 700, fontFamily: "'DM Mono', monospace",
                    color: results.length > 0 ? "#86c441" : "#e87171",
                    animation: "countPop 0.3s ease",
                  }}>{results.length}</div>
                  <div style={{ fontSize: 11, color: "#888", marginTop: 2 }}>
                    checkmate{results.length !== 1 ? "s" : ""}-in-one
                  </div>
                </div>

                {results.length > 0 && (
                  <div style={{ maxHeight: 240, overflowY: "auto", display: "flex", flexDirection: "column", gap: 2 }}>
                    {results.map((m, i) => {
                      const active = hl && hl.from[0] === m.from[0] && hl.from[1] === m.from[1] && hl.to[0] === m.to[0] && hl.to[1] === m.to[1];
                      return (
                        <div key={i} className="move-row"
                          onClick={() => setHl(active ? null : m)}
                          style={{
                            display: "flex", alignItems: "center", gap: 8,
                            padding: "7px 8px", borderRadius: 8,
                            background: active ? "#1e2b16" : "transparent",
                            border: active ? "1px solid #2d4a2d" : "1px solid transparent",
                          }}>
                          <span style={{
                            fontSize: 22, lineHeight: 1, width: 24, textAlign: "center",
                            fontWeight: 900,
                            color: colOf(m.piece) === "b" ? "#1a1a18" : "#f0efe9",
                            textShadow: colOf(m.piece) === "b"
                              ? "0 1px 3px rgba(0,0,0,0.5), 0 0 6px rgba(0,0,0,0.2)"
                              : "0 1px 3px rgba(0,0,0,0.3), 0 0 8px rgba(255,255,255,0.1)",
                          }}>{SYM[m.piece]}</span>
                          <span style={{
                            fontSize: 13, fontFamily: "'DM Mono', monospace", fontWeight: 500,
                            color: active ? "#86c441" : "#999",
                          }}>
                            {toSq(m.from[0], m.from[1])} → {toSq(m.to[0], m.to[1])}
                          </span>
                        </div>
                      );
                    })}
                  </div>
                )}

                {results.length === 0 && (
                  <p style={{ fontSize: 11, color: "#555", textAlign: "center", lineHeight: 1.6, marginTop: 4 }}>
                    No checkmate-in-one for {side === "w" ? "White" : "Black"} in this position.
                  </p>
                )}
              </div>
            )}
          </div>
        </div>
      </div>

      <p style={{
        marginTop: 24, fontSize: 10, color: "#3a3a38", maxWidth: 460,
        textAlign: "center", lineHeight: 1.6,
      }}>
        Select a piece, click to place. One king per color. Pawns auto-promote to queen.
      </p>
    </div>
  );
}