#include <pebble.h>
#include "locale.h"

// See locale.h. One table per language, all indexed by MsgId; the active one is
// chosen once at startup from the system locale's two-letter prefix. English is
// the default and the fallback for any missing entry. Source is UTF-8 and the
// Pebble system fonts cover Western-European accents (é è ñ ü ç ã õ í ó), so the
// translations render as written.

typedef enum {
  LANG_EN = 0, LANG_IT, LANG_ES, LANG_FR, LANG_DE, LANG_PT, LANG_COUNT
} Lang;

static Lang s_lang = LANG_EN;

static const char *const STRINGS[LANG_COUNT][MSG_COUNT] = {
  [LANG_EN] = {
    [MSG_UNIT_MINUTES]  = "minutes",
    [MSG_QUIET_ACTIVE]  = "Quiet Time on",
    [MSG_CONFIRM_STOP]  = "Back = stop",
    [MSG_SESSION_ENDED] = "Session ended",
    [MSG_START]         = "Start session",
    [MSG_HOME_SUMMARY]     = "%d min, a tick every %d min",
    [MSG_HOME_SUMMARY_OFF] = "%d min, no ticks",
    [MSG_DURATION]      = "Duration",
    [MSG_TICK_INTERVAL] = "Tick interval",
    [MSG_VIBE_STRENGTH] = "Vibration",
    [MSG_VIBE_LIGHT]    = "Light",
    [MSG_VIBE_MEDIUM]   = "Medium",
    [MSG_VIBE_STRONG]   = "Strong",
    [MSG_TOTAL_HOURS]   = "You've meditated %s hours",
    [MSG_TOTAL_MIN]     = "You've meditated %d min",
    [MSG_QUIET_TITLE]   = "Turn on Quiet Time",
    [MSG_QUIET_BODY]    = "so you won't be disturbed",
    [MSG_QUIET_HINT]    = "Select = start anyway",
  },
  [LANG_IT] = {
    [MSG_UNIT_MINUTES]  = "minuti",
    [MSG_QUIET_ACTIVE]  = "Quiet Time attivo",
    [MSG_CONFIRM_STOP]  = "Indietro = termina",
    [MSG_SESSION_ENDED] = "Seduta terminata",
    [MSG_START]         = "Inizia seduta",
    [MSG_HOME_SUMMARY]     = "%d min, tick ogni %d min",
    [MSG_HOME_SUMMARY_OFF] = "%d min, senza tick",
    [MSG_DURATION]      = "Durata",
    [MSG_TICK_INTERVAL] = "Intervallo tick",
    [MSG_VIBE_STRENGTH] = "Vibrazione",
    [MSG_VIBE_LIGHT]    = "Leggero",
    [MSG_VIBE_MEDIUM]   = "Medio",
    [MSG_VIBE_STRONG]   = "Forte",
    [MSG_TOTAL_HOURS]   = "Hai meditato per %s ore",
    [MSG_TOTAL_MIN]     = "Hai meditato per %d min",
    [MSG_QUIET_TITLE]   = "Attiva Quiet Time",
    [MSG_QUIET_BODY]    = "per non essere disturbato",
    [MSG_QUIET_HINT]    = "Select = inizia comunque",
  },
  [LANG_ES] = {
    [MSG_UNIT_MINUTES]  = "minutos",
    [MSG_QUIET_ACTIVE]  = "Quiet Time activo",
    [MSG_CONFIRM_STOP]  = "Atrás = terminar",
    [MSG_SESSION_ENDED] = "Sesión terminada",
    [MSG_START]         = "Empezar sesión",
    [MSG_HOME_SUMMARY]     = "%d min, tick cada %d min",
    [MSG_HOME_SUMMARY_OFF] = "%d min, sin tick",
    [MSG_DURATION]      = "Duración",
    [MSG_TICK_INTERVAL] = "Intervalo tick",
    [MSG_VIBE_STRENGTH] = "Vibración",
    [MSG_VIBE_LIGHT]    = "Suave",
    [MSG_VIBE_MEDIUM]   = "Medio",
    [MSG_VIBE_STRONG]   = "Fuerte",
    [MSG_TOTAL_HOURS]   = "Has meditado %s horas",
    [MSG_TOTAL_MIN]     = "Has meditado %d min",
    [MSG_QUIET_TITLE]   = "Activa Quiet Time",
    [MSG_QUIET_BODY]    = "para no ser molestado",
    [MSG_QUIET_HINT]    = "Select = empezar igual",
  },
  [LANG_FR] = {
    [MSG_UNIT_MINUTES]  = "minutes",
    [MSG_QUIET_ACTIVE]  = "Quiet Time activé",
    [MSG_CONFIRM_STOP]  = "Retour = arrêter",
    [MSG_SESSION_ENDED] = "Séance terminée",
    [MSG_START]         = "Démarrer séance",
    [MSG_HOME_SUMMARY]     = "%d min, tick toutes les %d min",
    [MSG_HOME_SUMMARY_OFF] = "%d min, sans tick",
    [MSG_DURATION]      = "Durée",
    [MSG_TICK_INTERVAL] = "Intervalle tick",
    [MSG_VIBE_STRENGTH] = "Vibration",
    [MSG_VIBE_LIGHT]    = "Léger",
    [MSG_VIBE_MEDIUM]   = "Moyen",
    [MSG_VIBE_STRONG]   = "Fort",
    [MSG_TOTAL_HOURS]   = "Tu as médité %s heures",
    [MSG_TOTAL_MIN]     = "Tu as médité %d min",
    [MSG_QUIET_TITLE]   = "Activez Quiet Time",
    [MSG_QUIET_BODY]    = "pour ne pas être dérangé",
    [MSG_QUIET_HINT]    = "Select = démarrer quand même",
  },
  [LANG_DE] = {
    [MSG_UNIT_MINUTES]  = "Minuten",
    [MSG_QUIET_ACTIVE]  = "Quiet Time an",
    [MSG_CONFIRM_STOP]  = "Zurück = beenden",
    [MSG_SESSION_ENDED] = "Sitzung beendet",
    [MSG_START]         = "Sitzung starten",
    [MSG_HOME_SUMMARY]     = "%d min, Tick alle %d min",
    [MSG_HOME_SUMMARY_OFF] = "%d min, kein Tick",
    [MSG_DURATION]      = "Dauer",
    [MSG_TICK_INTERVAL] = "Tick-Intervall",
    [MSG_VIBE_STRENGTH] = "Vibration",
    [MSG_VIBE_LIGHT]    = "Leicht",
    [MSG_VIBE_MEDIUM]   = "Mittel",
    [MSG_VIBE_STRONG]   = "Stark",
    [MSG_TOTAL_HOURS]   = "%s Stunden meditiert",
    [MSG_TOTAL_MIN]     = "%d min meditiert",
    [MSG_QUIET_TITLE]   = "Quiet Time aktivieren",
    [MSG_QUIET_BODY]    = "um nicht gestört zu werden",
    [MSG_QUIET_HINT]    = "Select = trotzdem starten",
  },
  [LANG_PT] = {
    [MSG_UNIT_MINUTES]  = "minutos",
    [MSG_QUIET_ACTIVE]  = "Quiet Time ativo",
    [MSG_CONFIRM_STOP]  = "Voltar = terminar",
    [MSG_SESSION_ENDED] = "Sessão terminada",
    [MSG_START]         = "Começar sessão",
    [MSG_HOME_SUMMARY]     = "%d min, tick a cada %d min",
    [MSG_HOME_SUMMARY_OFF] = "%d min, sem tick",
    [MSG_DURATION]      = "Duração",
    [MSG_TICK_INTERVAL] = "Intervalo tick",
    [MSG_VIBE_STRENGTH] = "Vibração",
    [MSG_VIBE_LIGHT]    = "Leve",
    [MSG_VIBE_MEDIUM]   = "Médio",
    [MSG_VIBE_STRONG]   = "Forte",
    [MSG_TOTAL_HOURS]   = "Você meditou %s horas",
    [MSG_TOTAL_MIN]     = "Você meditou %d min",
    [MSG_QUIET_TITLE]   = "Ative Quiet Time",
    [MSG_QUIET_BODY]    = "para não ser incomodado",
    [MSG_QUIET_HINT]    = "Select = começar mesmo assim",
  },
};

// Map a locale's two-letter prefix to a language. Anything not listed (and any
// null/short locale) stays on the English default.
static const struct { char code[2]; Lang lang; } LANG_MAP[] = {
  { {'i','t'}, LANG_IT },
  { {'e','s'}, LANG_ES },
  { {'f','r'}, LANG_FR },
  { {'d','e'}, LANG_DE },
  { {'p','t'}, LANG_PT },
};

void locale_init(void) {
  const char *loc = i18n_get_system_locale();   // e.g. "en_US", "it_IT"
  s_lang = LANG_EN;
  if (!loc || !loc[0] || !loc[1]) return;
  for (unsigned i = 0; i < sizeof(LANG_MAP) / sizeof(LANG_MAP[0]); i++) {
    if (loc[0] == LANG_MAP[i].code[0] && loc[1] == LANG_MAP[i].code[1]) {
      s_lang = LANG_MAP[i].lang;
      return;
    }
  }
}

const char *L(MsgId id) {
  if (id >= MSG_COUNT) return "";   // MsgId is unsigned; >= catches any invalid value
  const char *s = STRINGS[s_lang][id];
  if (!s) s = STRINGS[LANG_EN][id];   // fall back to English on any gap
  return s ? s : "";
}

const char *locale_decimal_sep(void) {
  return s_lang == LANG_EN ? "." : ",";
}
