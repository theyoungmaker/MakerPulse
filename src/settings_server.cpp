#include "settings_server.h"
#include "display.h"



void appendTimezoneOptions(String& page, const String& selectedKey) {
  struct TimezoneGroup {
    const char* label;
    const char* keys[8];
    int count;
  };

  const TimezoneGroup groups[] = {
    {"Global", {"utc"}, 1},
    {"Europe", {"atlantic_azores", "europe_west", "uk", "europe_central", "europe_east"}, 5},
    {"Africa & Middle East", {"africa_south", "middle_east_gulf"}, 2},
    {"Asia", {"singapore", "india", "thailand", "china", "asia_tokyo", "korea"}, 6},
    {"North America", {"us_eastern", "us_central", "us_mountain", "us_arizona", "us_pacific", "alaska", "hawaii", "canada_atlantic"}, 8},
    {"South America", {"brazil_east", "argentina"}, 2},
    {"Australia & Oceania", {"australia_perth", "australia_darwin", "australia_sydney", "new_zealand"}, 4}
  };

  for (const TimezoneGroup& group : groups) {
    page += "<optgroup label='";
    page += group.label;
    page += "'>";
    for (int i = 0; i < group.count; i++) {
      const char* key = group.keys[i];
      page += "<option value='";
      page += key;
      page += "'";
      if (selectedKey == key) page += " selected";
      page += ">";
      page += timezoneLabelByKey(key);
      page += "</option>";
    }
    page += "</optgroup>";
  }
}


void appendHomeWidgetOptions(String& page, const String& selectedKey) {
  const HomeWidgetType types[] = {
    HOME_WIDGET_WEEK,
    HOME_WIDGET_TIMER,
    HOME_WIDGET_RAIN,
    HOME_WIDGET_OUTDOOR,
    HOME_WIDGET_KP,
    HOME_WIDGET_UV,
    HOME_WIDGET_WIND,
    HOME_WIDGET_SUN,
    HOME_WIDGET_PSI,
    HOME_WIDGET_BUS
  };

  for (HomeWidgetType type : types) {
    const char* key = homeWidgetKey(type);
    page += "<option value='";
    page += key;
    page += "'";
    if (selectedKey == key) page += " selected";
    page += ">";
    page += homeWidgetLabel(type);
    page += "</option>";
  }
}


static String htmlEscape(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}


static String cssColorFrom565(uint16_t color) {
  uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((color >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (color & 0x1F) * 255 / 31;
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return String(buf);
}


static String accentPreviewCss(const String& key) {
  if (key == "standard") return cssColorFrom565(0xEF7D);
  if (key == "ice")      return cssColorFrom565(0xEFFF);
  if (key == "white")    return cssColorFrom565(TFT_WHITE);
  if (key == "cyan")     return cssColorFrom565(0x5EFA);
  if (key == "mint")     return cssColorFrom565(0x07F0);
  if (key == "green")    return cssColorFrom565(TFT_GREEN);
  if (key == "blue")     return cssColorFrom565(0x3D9F);
  if (key == "purple")   return cssColorFrom565(0xA2F5);
  if (key == "pink")     return cssColorFrom565(0xF97F);
  if (key == "orange")   return cssColorFrom565(0xFD20);
  if (key == "amber")    return cssColorFrom565(0xFEA0);
  if (key == "red")      return cssColorFrom565(TFT_RED);
  return cssColorFrom565(0xEF7D);
}


static String themePreviewCss(const String& key) {
  if (key == "slate")    return cssColorFrom565(0x08A3);
  if (key == "deep")     return cssColorFrom565(0x0000);
  if (key == "nordic")   return cssColorFrom565(0x0864);
  if (key == "forest")   return cssColorFrom565(0x0208);
  if (key == "coffee")   return cssColorFrom565(0x18A3);
  if (key == "soft")     return cssColorFrom565(0x10A2);
  if (key == "midnight") return cssColorFrom565(0x0008);
  if (key == "graphite") return cssColorFrom565(0x1082);
  if (key == "garnet")   return cssColorFrom565(0x1004);
  if (key == "ochre")    return cssColorFrom565(0x20E1);
  return cssColorFrom565(0x08A3);
}


// =========================================================
// WEB SERVER
// =========================================================
void handleRoot() {
  String accent = prefs.getString("accent", "cyan");
  String bg     = prefs.getString("bg", "slate");
  String txt    = prefs.getString("text", "standard");
  // Each color key is either a named preset or "custom:#RRGGBB" from the
  // color-wheel input; resolve either form to the hex the page should show.
  String accentHex = accent.startsWith("custom:") ? accent.substring(7) : accentPreviewCss(accent);
  String textHex   = txt.startsWith("custom:")    ? txt.substring(7)    : accentPreviewCss(txt);
  String units  = prefs.getString("units", "metric");
  String region = prefs.getString("region", "europe");
  String tz     = sanitizeTimezoneKey(prefs.getString("tz", "singapore"));
  String nickname = prefs.getString("nickname", "");
  bool flashMode = prefs.getBool("flashMode", false);
  String homeSlotKeys[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeSlotKeys[i] = prefs.getString((String("homeSlot") + String(i)).c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }

  String page;
  page.reserve(21000);

  page += "<!doctype html><html><head>";
  page += "<meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>MakerPulse</title>";
  page += "<style>";
  page += ":root{color-scheme:light;}";
  page += "body{margin:0;background:linear-gradient(180deg,#fff4cd 0%,#fffaf0 36%,#ffffff 100%);color:#3a3a3a;font-family:system-ui,sans-serif;}";
  page += "a{color:#b9830a;}";
  page += ".wrap{max-width:980px;margin:0 auto;padding:28px 16px 36px;}";
  page += ".hero{margin-bottom:18px;padding:18px 20px;border:1px solid #ffe3a3;border-radius:20px;background:linear-gradient(135deg,#fff8e6 0%,#ffffff 100%);box-shadow:0 10px 30px rgba(255,182,7,.15);}";
  page += ".hero-brand{display:flex;align-items:center;gap:14px;margin-bottom:10px;}";
  page += ".hero-brand svg{width:40px;height:52px;flex-shrink:0;}";
  page += ".hero-titles{display:flex;flex-direction:column;}";
  page += ".hero-by{font-size:12px;font-weight:700;color:#c79a2e;letter-spacing:.04em;text-transform:uppercase;}";
  page += ".hero h1{font-size:30px;margin:0;color:#262220;}";
  page += ".hero p{margin:0;color:#7a7a7a;font-size:14px;}";
  page += ".ip{display:inline-block;margin-top:14px;padding:8px 12px;border-radius:999px;background:#fff7e0;border:1px solid #ffdf94;color:#8a6d1f;font-size:13px;}";
  page += ".layout{display:grid;grid-template-columns:1.15fr .85fr;gap:16px;align-items:start;}";
  page += ".stack{display:grid;gap:16px;}";
  page += ".panel{background:#ffffff;border:1px solid #f0e6cf;border-radius:18px;padding:18px;margin:0;box-shadow:0 1px 2px rgba(0,0,0,.04);}";
  page += ".panel-toggle{width:100%;display:flex;align-items:center;justify-content:space-between;gap:12px;background:none;border:none;color:#262220;padding:0;margin:0;cursor:pointer;text-align:left;}";
  page += ".panel-toggle:hover{color:#000000;}";
  page += ".panel-toggle h2{flex:1;}";
  page += ".panel-chevron{font-size:18px;color:#c79a2e;transition:transform .18s ease;}";
  page += ".panel.collapsed .panel-chevron{transform:rotate(-90deg);}";
  page += ".panel-body{margin-top:12px;}";
  page += ".panel.collapsed .panel-body{display:none;}";
  page += ".panel h2{margin:0 0 6px 0;font-size:18px;}";
  page += ".panel p{margin:0 0 14px 0;color:#7a7a7a;font-size:13px;line-height:1.45;}";
  page += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;}";
  page += ".grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:14px;}";
  page += ".label{display:block;font-size:13px;margin:0 0 8px 0;color:#54595f;font-weight:600;}";
  page += "input,select{width:100%;border-radius:12px;border:1px solid #e7decb;background:#fffdf8;color:#262220;padding:12px;box-sizing:border-box;font:inherit;}";
  page += "button{margin-top:18px;background:#ffb607;border:none;color:#241b00;padding:13px 18px;border-radius:12px;font-weight:800;cursor:pointer;font:inherit;}";
  page += "button:hover{background:#e6a400;}";
  page += ".muted{font-size:13px;color:#7a7a7a;line-height:1.45;}";
  page += ".footer-note{margin-top:10px;font-size:12px;color:#9a9488;}";
  page += ".settings-block{margin-top:18px;padding-top:16px;border-top:1px solid #f0e6cf;}";
  page += ".settings-block:first-of-type{margin-top:0;padding-top:0;border-top:none;}";
  page += ".settings-title{display:block;margin:0 0 6px 0;font-size:14px;font-weight:700;color:#262220;letter-spacing:.02em;}";
  page += ".settings-desc{margin:0 0 12px 0;font-size:12px;color:#7a7a7a;line-height:1.45;}";
  page += ".color-stack{display:grid;gap:12px;}";
  page += ".color-row{display:grid;grid-template-columns:120px 1fr;gap:12px;align-items:center;}";
  page += ".color-meta{display:flex;align-items:center;justify-content:space-between;gap:10px;}";
  page += ".color-meta .label{margin:0;color:#262220;}";
  page += ".color-value{font-size:12px;color:#7a7a7a;white-space:nowrap;}";
  page += ".swatch-row{display:flex;flex-wrap:wrap;gap:8px;}";
  page += ".swatch{width:22px;height:22px;border-radius:999px;border:1px solid rgba(0,0,0,.15);cursor:pointer;position:relative;box-sizing:border-box;}";
  page += ".swatch input{display:none;}";
  page += ".color-wheel{-webkit-appearance:none;appearance:none;width:22px;height:22px;padding:0;border:1px solid rgba(0,0,0,.15);border-radius:999px;cursor:pointer;background:none;}";
  page += ".color-wheel::-webkit-color-swatch-wrapper{padding:0;}";
  page += ".color-wheel::-webkit-color-swatch{border:none;border-radius:999px;}";
  page += ".swatch.active{box-shadow:0 0 0 2px #61ce70, 0 0 0 5px rgba(97,206,112,.18);}";
  page += ".swatch.active::after{content:'';position:absolute;inset:5px;border-radius:999px;border:1px solid rgba(0,16,24,.45);}";
  page += ".timer-slot-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-top:14px;}";
  page += ".timer-slot{border:1px solid #e7decb;border-radius:12px;background:#fffdf8;padding:10px 10px 12px 10px;}";
  page += ".timer-slot-head{font-size:12px;color:#7a7a7a;margin-bottom:8px;font-weight:600;}";
  page += ".timer-slot-input{display:flex;align-items:center;gap:8px;}";
  page += ".timer-slot input{padding:10px 12px;text-align:center;font-weight:700;}";
  page += ".timer-unit{font-size:12px;color:#7a7a7a;white-space:nowrap;}";
  page += ".ym-footer{margin-top:22px;text-align:center;font-size:12px;color:#9a9488;}";
  page += ".ym-footer a{color:#c79a2e;font-weight:700;text-decoration:none;}";
  page += ".ym-footer a:hover{text-decoration:underline;}";
  page += "@media(max-width:820px){.layout{grid-template-columns:1fr;}.grid,.grid-3,.timer-slot-grid{grid-template-columns:1fr;}.color-row{grid-template-columns:1fr;}}";
  page += "</style></head><body><div class='wrap'>";
  page += "<div class='hero'>";
  page += "<div class='hero-brand'>" YM_LOGO_SVG "<div class='hero-titles'><h1>MakerPulse</h1><span class='hero-by'>by The Young Maker</span></div></div>";
  page += "<p>Shape MakerPulse into your own desk companion with widgets, notes, colors, and smart daily tools.</p>";
  page += "<div class='ip'>ESP IP: ";
  page += WiFi.localIP().toString();
  page += "</div></div>";

  page += "<form method='POST' action='/save'>";
  page += "<div class='layout'><div class='stack'>";

  page += "<div class='panel' data-panel='theme'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Theme and color</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Colors and visual style for the display.</p>";
  page += "<div class='grid'>";

  page += "<div style='grid-column:1 / -1;' class='color-stack'>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Accent</label><span class='color-value' id='accent-value'>";
  page += accent;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(accent=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='accent' value='standard'" + String(accent=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='accent' value='ice'" + String(accent=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='accent' value='white'" + String(accent=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='accent' value='cyan'" + String(accent=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='accent' value='mint'" + String(accent=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='accent' value='green'" + String(accent=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='accent' value='blue'" + String(accent=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='accent' value='purple'" + String(accent=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='accent' value='pink'" + String(accent=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='accent' value='orange'" + String(accent=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='accent' value='amber'" + String(accent=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='accent' value='red'" + String(accent=="red"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent.startsWith("custom:")?" active":"") + "' id='accent-custom-swatch' style='background:" + accentHex + ";'><input type='radio' name='accent' value='custom'" + String(accent.startsWith("custom:")?" checked":"") + "></label>";
  page += "<input type='color' class='color-wheel' value='" + accentHex + "' oninput=\"document.getElementById('accent-custom-swatch').style.background=this.value;document.getElementById('accent-custom-swatch').querySelector('input').checked=true;document.getElementById('accent-custom-swatch').querySelector('input').dispatchEvent(new Event('change'));document.getElementById('accent-custom-hex').value=this.value;\">";
  page += "<input type='hidden' name='accentCustom' id='accent-custom-hex' value='" + accentHex + "'>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Text</label><span class='color-value' id='text-value'>";
  page += txt;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(txt=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='text' value='standard'" + String(txt=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='text' value='ice'" + String(txt=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='text' value='white'" + String(txt=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='text' value='cyan'" + String(txt=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='text' value='mint'" + String(txt=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='text' value='green'" + String(txt=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='text' value='blue'" + String(txt=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='text' value='purple'" + String(txt=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='text' value='pink'" + String(txt=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='text' value='orange'" + String(txt=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='text' value='amber'" + String(txt=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='text' value='red'" + String(txt=="red"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt.startsWith("custom:")?" active":"") + "' id='text-custom-swatch' style='background:" + textHex + ";'><input type='radio' name='text' value='custom'" + String(txt.startsWith("custom:")?" checked":"") + "></label>";
  page += "<input type='color' class='color-wheel' value='" + textHex + "' oninput=\"document.getElementById('text-custom-swatch').style.background=this.value;document.getElementById('text-custom-swatch').querySelector('input').checked=true;document.getElementById('text-custom-swatch').querySelector('input').dispatchEvent(new Event('change'));document.getElementById('text-custom-hex').value=this.value;\">";
  page += "<input type='hidden' name='textCustom' id='text-custom-hex' value='" + textHex + "'>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>Theme</label><span class='color-value' id='bg-value'>";
  page += bg;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(bg=="slate"?" active":"") + "' style='background:" + themePreviewCss("slate") + ";'><input type='radio' name='bg' value='slate'" + String(bg=="slate"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="deep"?" active":"") + "' style='background:" + themePreviewCss("deep") + ";'><input type='radio' name='bg' value='deep'" + String(bg=="deep"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="nordic"?" active":"") + "' style='background:" + themePreviewCss("nordic") + ";'><input type='radio' name='bg' value='nordic'" + String(bg=="nordic"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="forest"?" active":"") + "' style='background:" + themePreviewCss("forest") + ";'><input type='radio' name='bg' value='forest'" + String(bg=="forest"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="coffee"?" active":"") + "' style='background:" + themePreviewCss("coffee") + ";'><input type='radio' name='bg' value='coffee'" + String(bg=="coffee"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="soft"?" active":"") + "' style='background:" + themePreviewCss("soft") + ";'><input type='radio' name='bg' value='soft'" + String(bg=="soft"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="midnight"?" active":"") + "' style='background:" + themePreviewCss("midnight") + ";'><input type='radio' name='bg' value='midnight'" + String(bg=="midnight"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="graphite"?" active":"") + "' style='background:" + themePreviewCss("graphite") + ";'><input type='radio' name='bg' value='graphite'" + String(bg=="graphite"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="garnet"?" active":"") + "' style='background:" + themePreviewCss("garnet") + ";'><input type='radio' name='bg' value='garnet'" + String(bg=="garnet"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="ochre"?" active":"") + "' style='background:" + themePreviewCss("ochre") + ";'><input type='radio' name='bg' value='ochre'" + String(bg=="ochre"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "</div>";

  page += "</div></div></div>";

  page += "<div class='panel' data-panel='settings'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Settings</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Core behavior and timer setup.</p>";
  page += "<div class='settings-block'>";
  page += "<span class='settings-title'>General</span>";
  page += "<div class='grid'>";
  page += "<div><label class='label'>MakerPulse nickname</label><input name='nickname' maxlength='24' value='" + htmlEscape(nickname) + "'></div>";
  page += "<div><label class='label'>Auto sleep interval</label><select name='sleepMin'>";
  page += "<option value='0'"  + String(sleepIntervalMin==0?" selected":"")  + ">Never</option>";
  page += "<option value='1'"  + String(sleepIntervalMin==1?" selected":"")  + ">1 minute</option>";
  page += "<option value='5'"  + String(sleepIntervalMin==5?" selected":"")  + ">5 minutes</option>";
  page += "<option value='10'" + String(sleepIntervalMin==10?" selected":"") + ">10 minutes</option>";
  page += "<option value='30'" + String(sleepIntervalMin==30?" selected":"") + ">30 minutes</option>";
  page += "<option value='60'" + String(sleepIntervalMin==60?" selected":"") + ">1 hour</option>";
  page += "</select><div class='muted' style='margin-top:8px;'>Sleep dims the screen first, then turns it fully off after 60 seconds.</div></div>";
  page += "<div><label class='label'>Measurement system</label><select name='units'>";
  page += "<option value='metric'"   + String(units=="metric"?" selected":"")   + ">Celsius / mm</option>";
  page += "<option value='imperial'" + String(units=="imperial"?" selected":"") + ">Fahrenheit / inches</option>";
  page += "</select></div>";
  page += "<div><label class='label'>Date format</label><select name='region'>";
  page += "<option value='europe'" + String(region=="europe"?" selected":"") + ">European: dd.mm.yyyy</option>";
  page += "<option value='us'" + String(region=="us"?" selected":"") + ">US: mm/dd/yyyy</option>";
  page += "</select></div>";
  page += "<div><label class='label'>Time zone</label><select name='tz'>";
  appendTimezoneOptions(page, tz);
  page += "</select></div>";
  page += "</div>";
  page += "</div>";
  page += "<div class='settings-block'><span class='settings-title'>Timer</span><div class='settings-desc'>Choose the six quick timers shown in the popup menu.</div><div class='timer-slot-grid'>";
  for (int i = 0; i < 6; i++) {
    page += "<div class='timer-slot'><div class='timer-slot-head'>Slot " + String(i + 1) + "</div><div class='timer-slot-input'><input type='number' min='1' max='180' name='timer" + String(i) + "' value='" + String(timerPresetMin[i]) + "'><span class='timer-unit'>min</span></div></div>";
  }
  page += "</div>";
  page += "<div style='margin-top:14px;'><span class='settings-title'>Alert behavior</span><label style='display:flex;align-items:center;gap:10px;color:#262220;'><input type='checkbox' name='flashMode' value='1'" + String(flashMode ? " checked" : "") + " style='width:auto;'>Flash screen when timer ends</label></div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Location</span><div class='settings-desc'>Used for weather data and sun times.</div><div class='grid-3'>";
  page += "<div><label class='label'>Location name</label><input name='locname' value='" + htmlEscape(locationName) + "'></div>";
  page += "<div><label class='label'>Latitude</label><input name='lat' value='" + String(LAT, 6) + "'></div>";
  page += "<div><label class='label'>Longitude</label><input name='lng' value='" + String(LNG, 6) + "'></div>";
  page += "</div><div class='footer-note'>Example Singapore: latitude 1.3521, longitude 103.8198.</div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Air quality (PSI)</span><div class='settings-desc'>NEA haze reading shown on the Weather page and as a Home widget. No API key needed.</div><div class='grid'>";
  page += "<div><label class='label'>PSI region</label><select name='psiRegion'>";
  page += "<option value='central'" + String(psiRegion=="central"?" selected":"") + ">Central</option>";
  page += "<option value='north'"   + String(psiRegion=="north"?" selected":"")   + ">North</option>";
  page += "<option value='south'"   + String(psiRegion=="south"?" selected":"")   + ">South</option>";
  page += "<option value='east'"    + String(psiRegion=="east"?" selected":"")    + ">East</option>";
  page += "<option value='west'"    + String(psiRegion=="west"?" selected":"")    + ">West</option>";
  page += "</select></div>";
  page += "</div></div>";
  page += "<div class='settings-block'><span class='settings-title'>Bus arrival</span><div class='settings-desc'>Shows next bus times for up to " + String(BUS_MAX_STOPS) + " stops, via LTA DataMall. MakerPulse works out of the box using a shared default key, but shared keys can hit rate limits &mdash; for reliable results, request your own free AccountKey at <a href='https://datamall.lta.gov.sg/content/datamall/en/request-for-api.html' target='_blank'>datamall.lta.gov.sg</a> and enter it below. Find each 5-digit bus stop code on the physical bus stop sign (you can also enter a code directly on the device from the Bus tab). The stop name is looked up automatically once a code is saved; use the override field if you'd rather set your own. Leave service number blank to show the soonest bus of any route.</div>";
  page += "<div><label class='label'>LTA AccountKey</label><input name='ltaKey' value='" + htmlEscape(ltaApiKey) + "'></div>";
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    String autoName = busStopDisplayName(i);
    page += "<div class='timer-slot' style='margin-top:12px;'>";
    page += "<div class='timer-slot-head'>Stop " + String(i + 1) + (autoName.length() > 0 ? (" - " + htmlEscape(autoName)) : "") + "</div>";
    page += "<div class='grid-3'>";
    page += "<div><label class='label'>Bus stop code</label><input name='busCode" + String(i) + "' maxlength='10' value='" + htmlEscape(busStops[i].code) + "'></div>";
    page += "<div><label class='label'>Name override</label><input name='busLabel" + String(i) + "' maxlength='30' value='" + htmlEscape(busStops[i].label) + "'></div>";
    page += "<div><label class='label'>Service number</label><input name='busSvc" + String(i) + "' maxlength='10' value='" + htmlEscape(busStops[i].svcFilter) + "'></div>";
    page += "</div></div>";
  }
  page += "</div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='widgets'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Widget Customization</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>Choose which widgets appear in the four Home slots below the clock card.</p>";
  page += "<div class='grid'>";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    page += "<div><label class='label'>";
    page += homeSlotLabel(i);
    page += "</label><select name='homeSlot";
    page += String(i);
    page += "'>";
    appendHomeWidgetOptions(page, homeSlotKeys[i]);
    page += "</select></div>";
  }
  page += "</div>";
  page += "</div></div>";

  page += "</div><div class='stack'>";

  page += "<button type='submit'>Save to MakerPulse</button>";
  page += "</div></div></form>";
  page += "<div class='ym-footer'>Built by <a href='https://theyoungmaker.com' target='_blank' rel='noopener'>The Young Maker</a> &middot; <a href='https://theyoungmaker.com/3dlab/' target='_blank' rel='noopener'>3D Lab</a></div>";
  page += "<div class='ym-footer'>MakerPulse firmware v" FIRMWARE_VERSION "</div>";
  page += "<script>";
  page += "var colorNames={accent:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},text:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},bg:{slate:'Slate',deep:'Deep black',nordic:'Nordic blue',forest:'Forest',coffee:'Coffee',soft:'Soft dark',midnight:'Midnight',graphite:'Graphite',garnet:'Garnet',ochre:'Ochre'}};";
  page += "var panelStorageKey='makerpulse-panel-state-v1';";
  page += "document.querySelectorAll('.swatch input').forEach(function(input){";
  page += "input.addEventListener('change',function(){";
  page += "document.querySelectorAll('.swatch input[name=\"'+input.name+'\"]').forEach(function(peer){";
  page += "peer.closest('.swatch').classList.toggle('active', peer.checked);";
  page += "});";
  page += "var valueEl=document.getElementById(input.name+'-value');";
  page += "if(valueEl&&input.value==='custom'){var hexEl=document.getElementById(input.name+'-custom-hex');valueEl.textContent=hexEl?hexEl.value:'Custom';}";
  page += "else if(valueEl&&colorNames[input.name]&&colorNames[input.name][input.value]){valueEl.textContent=colorNames[input.name][input.value];}";
  page += "});";
  page += "});";
  page += "function readPanelState(){try{return JSON.parse(localStorage.getItem(panelStorageKey)||'{}');}catch(e){return {};}}";
  page += "function writePanelState(state){localStorage.setItem(panelStorageKey,JSON.stringify(state));}";
  page += "function applyPanelState(panel,collapsed){panel.classList.toggle('collapsed',collapsed);var btn=panel.querySelector('.panel-toggle');if(btn){btn.setAttribute('aria-expanded',collapsed?'false':'true');}}";
  page += "var savedPanelState=readPanelState();";
  page += "document.querySelectorAll('.panel[data-panel]').forEach(function(panel){";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(Object.prototype.hasOwnProperty.call(savedPanelState,panelId)){applyPanelState(panel,!!savedPanelState[panelId]);}";
  page += "});";
  page += "document.querySelectorAll('.panel-toggle').forEach(function(btn){";
  page += "btn.addEventListener('click',function(){";
  page += "var panel=btn.closest('.panel');";
  page += "var collapsed=!panel.classList.contains('collapsed');";
  page += "applyPanelState(panel,collapsed);";
  page += "var state=readPanelState();";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(panelId){state[panelId]=collapsed;writePanelState(state);}";
  page += "});";
  page += "});";
  page += "</script>";
  page += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", page);
}


void handleSave() {
  String newAccent = server.hasArg("accent") ? server.arg("accent") : "cyan";
  String newBg     = server.hasArg("bg") ? server.arg("bg") : "slate";
  String newText   = server.hasArg("text") ? server.arg("text") : "standard";
  // The color-wheel swatch posts accent/text="custom" plus the picked hex
  // in a sibling field; fold that into the stored key as "custom:#RRGGBB".
  if (newAccent == "custom") {
    String hex = server.hasArg("accentCustom") ? server.arg("accentCustom") : "#5EFA5E";
    newAccent = "custom:" + hex;
  }
  if (newText == "custom") {
    String hex = server.hasArg("textCustom") ? server.arg("textCustom") : "#EFEF7D";
    newText = "custom:" + hex;
  }
  String newUnits  = server.hasArg("units") ? server.arg("units") : "metric";
  String newRegion = server.hasArg("region") ? server.arg("region") : "europe";
  String newTz     = server.hasArg("tz") ? server.arg("tz") : timezoneKey;
  String newLoc    = server.hasArg("locname") ? server.arg("locname") : locationName;
  String newNickname = server.hasArg("nickname") ? server.arg("nickname") : deviceNickname;
  HomeWidgetType newHomeSlots[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    String currentKey = homeWidgetKey(homeWidgetSlots[i]);
    newHomeSlots[i] = homeWidgetFromKey(server.hasArg(key) ? server.arg(key) : currentKey);
  }

  float newLat = server.hasArg("lat") ? server.arg("lat").toFloat() : LAT;
  float newLng = server.hasArg("lng") ? server.arg("lng").toFloat() : LNG;

  String newPsiRegion = server.hasArg("psiRegion") ? server.arg("psiRegion") : psiRegion;
  String newLtaKey    = server.hasArg("ltaKey") ? server.arg("ltaKey") : ltaApiKey;

  BusStopConfig newBusStops[BUS_MAX_STOPS];
  bool busStopCodeChanged[BUS_MAX_STOPS];
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    String codeKey  = String("busCode") + String(i);
    String labelKey = String("busLabel") + String(i);
    String svcKey   = String("busSvc") + String(i);

    newBusStops[i].code      = server.hasArg(codeKey) ? server.arg(codeKey) : busStops[i].code;
    newBusStops[i].label     = server.hasArg(labelKey) ? server.arg(labelKey) : busStops[i].label;
    newBusStops[i].svcFilter = server.hasArg(svcKey) ? server.arg(svcKey) : busStops[i].svcFilter;

    newBusStops[i].code.trim();
    newBusStops[i].label.trim();
    newBusStops[i].svcFilter.trim();

    if (newBusStops[i].code.length() > 10) newBusStops[i].code = newBusStops[i].code.substring(0, 10);
    if (newBusStops[i].label.length() > 30) newBusStops[i].label = newBusStops[i].label.substring(0, 30);
    if (newBusStops[i].svcFilter.length() > 10) newBusStops[i].svcFilter = newBusStops[i].svcFilter.substring(0, 10);

    busStopCodeChanged[i] = (newBusStops[i].code != busStops[i].code);
  }

  newLoc.trim();
  newNickname.trim();
  newPsiRegion.trim();
  newLtaKey.trim();

  if (newLoc.length() == 0) newLoc = "Unknown";
  if (newNickname.length() > 24) newNickname = newNickname.substring(0, 24);
  if (newUnits != "metric" && newUnits != "imperial") newUnits = "metric";
  if (newRegion != "europe" && newRegion != "us") newRegion = "europe";
  newTz = sanitizeTimezoneKey(newTz);
  newPsiRegion = sanitizePsiRegion(newPsiRegion);

  int newSleepMin = server.hasArg("sleepMin") ? server.arg("sleepMin").toInt() : sleepIntervalMin;
  sleepIntervalMin = constrain(newSleepMin, 0, 120);
  bool newFlashMode = server.hasArg("flashMode");

  bool locationChanged =
    (fabsf(newLat - LAT) > 0.0001f) ||
    (fabsf(newLng - LNG) > 0.0001f) ||
    (newLoc != locationName);
  bool psiRegionChanged = (newPsiRegion != psiRegion);
  bool busSettingsChanged = (newLtaKey != ltaApiKey);
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    if (busStopCodeChanged[i] ||
        newBusStops[i].label != busStops[i].label ||
        newBusStops[i].svcFilter != busStops[i].svcFilter) {
      busSettingsChanged = true;
    }
  }

  deviceNickname = newNickname;
  locationName = newLoc;
  LAT = newLat;
  LNG = newLng;
  unitKey = newUnits;
  regionFormatKey = newRegion;
  timezoneKey = newTz;
  flashModeEnabled = newFlashMode;
  psiRegion = newPsiRegion;
  ltaApiKey = newLtaKey;
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    busStops[i] = newBusStops[i];
    if (busStopCodeChanged[i]) {
      busStopNames[i] = BusStopNameInfo();
      busDataValidByStop[i] = false;
      busServiceCountByStop[i] = 0;
      lastBusFetchByStop[i] = 0;
      busServicePage[i] = 0;
    }
  }
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeWidgetSlots[i] = newHomeSlots[i];
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    int currentValue = timerPresetMin[i];
    int nextValue = server.hasArg(key) ? server.arg(key).toInt() : currentValue;
    timerPresetMin[i] = sanitizeTimerMinutes(nextValue);
  }

  prefs.putString("accent", newAccent);
  prefs.putString("bg", newBg);
  prefs.putString("text", newText);
  prefs.putString("units", unitKey);
  prefs.putString("region", regionFormatKey);
  prefs.putString("tz", timezoneKey);
  prefs.putString("nickname", deviceNickname);
  prefs.putString("locname", locationName);
  prefs.putFloat("lat", LAT);
  prefs.putFloat("lng", LNG);
  prefs.putInt("sleepMin", sleepIntervalMin);
  prefs.putBool("flashMode", flashModeEnabled);
  prefs.putString("psiRegion", psiRegion);
  prefs.putString("ltaKey", ltaApiKey);
  for (int i = 0; i < BUS_MAX_STOPS; i++) {
    prefs.putString((String("busCode") + String(i)).c_str(), busStops[i].code);
    prefs.putString((String("busLabel") + String(i)).c_str(), busStops[i].label);
    prefs.putString((String("busSvc") + String(i)).c_str(), busStops[i].svcFilter);
    if (busStopCodeChanged[i]) {
      prefs.putString((String("busName") + String(i)).c_str(), "");
      prefs.putString((String("busRoad") + String(i)).c_str(), "");
    }
  }
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    prefs.putString(key.c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }
  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    prefs.putInt(key.c_str(), timerPresetMin[i]);
  }

  applyThemeByKey(newAccent, newBg);
  applyTextColorByKey(newText);
  applyDeviceTimezoneByKey(timezoneKey);
  if (!sleepDimmed && !sleepOff) setBacklight(BL_FULL);

  pageDirty = true;
  dataDirty = true;

  cacheClock = "";
  cacheHomeEmpty1 = "";
  cacheHomeEmpty2 = "";
  cacheFocusTimer = "";
  cacheTimerMenu = "";
  cacheTimerDone = "";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  lastNowText = "";
  lastHourlyText = "";
  lastPsiText = "";
  lastPsiLevelText = "";
  lastUptimeText = "";

  if (locationChanged) resetDataCaches();
  if (psiRegionChanged) {
    psiValue = NAN;
    lastPsiFetch = 0;
  }
  if (busSettingsChanged) {
    lastBusPageText = "";
  }

  server.sendHeader("Location", "/");
  server.send(303);
}


void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}
