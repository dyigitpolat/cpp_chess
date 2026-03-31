#include "html_export.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace chess {

namespace {

std::string html_escape(const std::string& s) {
  std::string o;
  o.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '&':
        o += "&amp;";
        break;
      case '<':
        o += "&lt;";
        break;
      case '>':
        o += "&gt;";
        break;
      case '"':
        o += "&quot;";
        break;
      default:
        o += c;
    }
  }
  return o;
}

const char* unicode_piece(uint8_t p) {
  switch (p) {
    case W_K:
      return "\u2654";
    case W_Q:
      return "\u2655";
    case W_R:
      return "\u2656";
    case W_B:
      return "\u2657";
    case W_N:
      return "\u2658";
    case B_K:
      return "\u265A";
    default:
      return "";
  }
}

}  // namespace

void write_results_html(const std::string& path, const Board& board, int mate_count,
                        const std::vector<MateMove>& mates, const std::string& title,
                        const MaterialSpec& material) {
  std::ostringstream body;
  body << "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"/><title>" << html_escape(title)
       << "</title>\n";
  body << "<link href=\"https://fonts.googleapis.com/css2?family=DM+Sans:wght@400;600;700&family=DM+Mono:wght@400;500&display=swap\" rel=\"stylesheet\"/>\n";
  body << "<style>\n";
  body << "*{box-sizing:border-box}body{margin:0;background:#0f0f0e;color:#e2e0da;font-family:'DM Sans',system-ui,sans-serif;padding:28px 12px;min-height:100vh;display:flex;flex-direction:column;align-items:center;}\n";
  body << "h1{font-size:22px;font-weight:700;color:#f0efe9;letter-spacing:-0.03em;margin:0 0 8px}\n";
  body << ".sub{font-size:13px;color:#666;margin-bottom:24px}\n";
  body << ".wrap{display:flex;gap:20px;flex-wrap:wrap;justify-content:center;align-items:flex-start;max-width:900px}\n";
  body << ".board-wrap{display:flex;flex-direction:column;align-items:center}\n";
  body << ".files{display:flex;margin-left:22px;margin-bottom:3px}\n";
  body << ".files div{width:48px;text-align:center;font-size:10px;color:#4a4a48;font-family:'DM Mono',monospace;font-weight:500}\n";
  body << ".board-row{display:flex}\n";
  body << ".ranks{width:16px;display:flex;flex-direction:column;margin-right:5px}\n";
  body << ".ranks div{height:48px;display:flex;align-items:center;justify-content:center;font-size:10px;color:#4a4a48;font-family:'DM Mono',monospace;font-weight:500}\n";
  body << ".grid{display:grid;grid-template-columns:repeat(8,48px);grid-template-rows:repeat(8,48px);border-radius:6px;overflow:hidden;box-shadow:0 8px 40px rgba(0,0,0,0.5),0 0 0 1px #2a2a28}\n";
  body << ".sq{width:48px;height:48px;display:flex;align-items:center;justify-content:center;font-size:33px;line-height:1;user-select:none;font-weight:900}\n";
  body << ".sq.light{background:#eae8e0}.sq.dark{background:#8b9b78}\n";
  body << ".w{color:#f0efe9;text-shadow:0 2px 4px rgba(0,0,0,0.3),0 0 10px rgba(255,255,255,0.12)}\n";
  body << ".b{color:#1a1a18;text-shadow:0 2px 4px rgba(0,0,0,0.5),0 0 8px rgba(0,0,0,0.25)}\n";
  body << ".panel{background:#191918;border-radius:14px;padding:14px 12px;border:1px solid #242422;min-width:200px;max-width:320px}\n";
  body << ".panel h2{font-size:10px;font-weight:600;text-transform:uppercase;letter-spacing:0.12em;color:#777;margin:0 0 12px}\n";
  body << ".count-box{text-align:center;padding:14px 0 10px;margin-bottom:10px;border-radius:10px;background:linear-gradient(135deg,#1a2e14,#1e3018);border:1px solid #2d4a2d}\n";
  body << ".count{font-size:40px;font-weight:700;font-family:'DM Mono',monospace;color:#86c441}\n";
  body << ".count-label{font-size:11px;color:#888;margin-top:2px}\n";
  body << ".moves{max-height:360px;overflow-y:auto;display:flex;flex-direction:column;gap:2px}\n";
  body << ".move-row{display:flex;align-items:center;gap:8px;padding:7px 8px;border-radius:8px;font-family:'DM Mono',monospace;font-size:13px;color:#999}\n";
  body << ".tag{font-size:10px;font-weight:600;text-transform:uppercase;letter-spacing:0.15em;color:#86c441;margin-bottom:6px;text-align:center}\n";
  body << "</style></head><body>\n";
  body << "<div class=\"tag\">Chess GA — Mate in one</div>\n";
  body << "<h1>" << html_escape(title) << "</h1>\n";
  body << "<p class=\"sub\">Same rules as chess.jsx <code>solve()</code>: legal quiet start, White to move, mate-in-one enumeration.</p>\n";
  body << "<div class=\"wrap\">\n";

  body << "<div class=\"board-wrap\"><div class=\"files\">";
  for (char f = 'a'; f <= 'h'; ++f) body << "<div>" << f << "</div>";
  body << "</div><div class=\"board-row\"><div class=\"ranks\">";
  for (int rank = 8; rank >= 1; --rank) body << "<div>" << rank << "</div>";
  body << "</div><div class=\"grid\">";

  for (int r = 0; r < 8; ++r)
    for (int c = 0; c < 8; ++c) {
      bool light = ((r + c) % 2) == 0;
      uint8_t p = board[rc_to_sq(r, c)];
      body << "<div class=\"sq " << (light ? "light" : "dark") << "\">";
      if (p != EMPTY) {
        bool white = is_white(p);
        body << "<span class=\"" << (white ? "w" : "b") << "\">" << unicode_piece(p) << "</span>";
      }
      body << "</div>";
    }

  body << "</div></div></div>\n";

  body << "<div class=\"panel\"><h2>Results</h2>\n";
  body << "<div class=\"count-box\"><div class=\"count\">" << mate_count << "</div>";
  body << "<div class=\"count-label\">checkmate" << (mate_count == 1 ? "" : "s") << "-in-one</div></div>\n";

  if (!mates.empty()) {
    body << "<div class=\"moves\">\n";
    for (const MateMove& m : mates) {
      body << "<div class=\"move-row\"><span class=\"" << (is_white(m.piece) ? "w" : "b") << "\" style=\"font-size:22px;width:24px;text-align:center\">"
           << unicode_piece(m.piece) << "</span> ";
      body << algebraic_from_sq(m.from) << " → " << algebraic_from_sq(m.to) << "</div>\n";
    }
    body << "</div>\n";
  }
  body << "</div></div>\n";
  body << "<p style=\"margin-top:24px;font-size:10px;color:#3a3a38;max-width:460px;text-align:center\">Exported by cpp_chess GA. Composition: "
       << html_escape(composition_string(material)) << ".</p>\n";
  body << "<script type=\"application/json\" id=\"board-data\">[";
  for (int i = 0; i < SQ_COUNT; ++i) {
    if (i) body << ",";
    body << static_cast<int>(board[static_cast<size_t>(i)]);
  }
  body << "]</script>\n";
  body << "<script>\n";
  body << "document.body.dataset.mateCount = " << mate_count << ";\n";
  body << "</script>\n";
  body << "</body></html>\n";

  std::ofstream out(path);
  out << body.str();
}

}  // namespace chess
