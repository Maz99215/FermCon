/**
 * FermCon v0.4.0 — Client API
 *
 * Module unique de communication HTTP avec le firmware FermCon.
 * Tous les endpoints API sont consommés exclusivement via ce module.
 *
 * Endpoints consommés :
 *   GET  /api/status          — État complet du système
 *   GET  /api/config           — Configuration sans les secrets
 *   POST /api/config           — Mise à jour partielle de la configuration
 *   POST /api/setpoint         — Consigne manuelle
 *   GET  /api/profile          — Lot (BatchResponse)
 *   POST /api/profile          — Enregistrement du lot (BatchSaveRequest)
 *   POST /api/profile/activate — Démarrage / arrêt du lot
 *   POST /api/restart          — Redémarrage du contrôleur
 *
 * Supprimé en v0.4.0 : GET et POST /api/fermentation (fusionnés dans le Lot).
 */

// ---------------------------------------------------------------------------
// Constantes (CONTRACTS §5.3)
// ---------------------------------------------------------------------------

/** Base des URLs API (même origine que la page). */
const API_BASE = '';

/** Cadence nominale du polling de /api/status (ms). */
export const POLL_INTERVAL_MS = 2000;

/** Plafond du backoff exponentiel (ms). */
export const POLL_BACKOFF_MAX_MS = 30000;

/** Facteur du backoff exponentiel. */
export const POLL_BACKOFF_FACTOR = 2;

/** Timeout d'abandon d'une requête (ms). */
const FETCH_TIMEOUT_MS = 5000;

/** Seuil de fraîcheur iSpindel (s), cohérent avec ISPINDEL_ONLINE_TIMEOUT_S. */
export const ISPINDEL_OFFLINE_S = 600;

// ---------------------------------------------------------------------------
// Helpers internes
// ---------------------------------------------------------------------------

/**
 * Exécute une requête fetch avec timeout et normalisation des erreurs.
 *
 * En cas de succès, retourne le corps JSON parsé (ou le texte brut si non-JSON).
 * En cas d'erreur, lève un objet normalisé :
 *   { status, code, message, field?, min?, max? }
 *
 * @param {string} url - URL relative de l'endpoint
 * @param {object} [options={}] - Options fetch (method, headers, body)
 * @returns {Promise<any>} Corps de la réponse
 * @throws {object} Erreur normalisée
 */
async function request(url, options = {}) {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), FETCH_TIMEOUT_MS);

  try {
    const response = await fetch(API_BASE + url, {
      ...options,
      signal: controller.signal,
      credentials: 'same-origin'
    });
    clearTimeout(timeoutId);

    // 401 : pas de corps JSON, message explicite, pas de boucle
    if (response.status === 401) {
      throw {
        status: 401,
        code: 'UNAUTHORIZED',
        message: 'Authentification requise — rechargez la page et saisissez vos identifiants'
      };
    }

    // Succès
    if (response.ok) {
      const contentType = response.headers.get('content-type') || '';
      if (contentType.includes('application/json')) {
        return await response.json();
      }
      return await response.text();
    }

    // Erreur : tenter de parser le corps JSON
    let body;
    try {
      body = await response.json();
    } catch (jsonError) {
      throw {
        status: response.status,
        code: 'HTTP_ERROR',
        message: 'Erreur HTTP ' + response.status
      };
    }

    // Corps JSON avec enveloppe ErrorResponse (CONTRACTS §3)
    if (body && body.error) {
      throw {
        status: response.status,
        code: body.error.code || 'UNKNOWN',
        message: body.error.message || 'Erreur inconnue',
        field: body.error.field,
        min: body.error.min,
        max: body.error.max
      };
    }

    // JSON sans champ error
    throw {
      status: response.status,
      code: 'HTTP_ERROR',
      message: 'Erreur HTTP ' + response.status
    };

  } catch (error) {
    clearTimeout(timeoutId);

    // Erreur déjà normalisée par notre code : la relayer telle quelle
    if (error && typeof error === 'object' && 'status' in error && 'code' in error) {
      throw error;
    }

    // Timeout (AbortController)
    if (error.name === 'AbortError') {
      throw {
        status: 0,
        code: 'TIMEOUT',
        message: 'La requête a expiré (délai de ' + (FETCH_TIMEOUT_MS / 1000) + 's)'
      };
    }

    // Erreur réseau ou autre
    throw {
      status: 0,
      code: 'NETWORK_ERROR',
      message: 'Impossible de joindre le contrôleur'
    };
  }
}

// ---------------------------------------------------------------------------
// API publique
// ---------------------------------------------------------------------------

/**
 * Récupère l'état complet du système.
 * GET /api/status — CONTRACTS §2.1
 * @returns {Promise<object>} StatusResponse
 */
export function getStatus() {
  return request('/api/status');
}

/**
 * Récupère la configuration sans les secrets, bornes incluses.
 * GET /api/config — CONTRACTS §2.2
 * @returns {Promise<object>} ConfigResponse avec bounds
 */
export function getConfig() {
  return request('/api/config');
}

/**
 * Met à jour partiellement la configuration.
 * POST /api/config — CONTRACTS §2.3
 * @param {object} partial - ConfigUpdateRequest, champs optionnels
 * @returns {Promise<object>} ConfigSaveResponse { status, reboot_required }
 */
export function saveConfig(partial) {
  return request('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(partial)
  });
}

/**
 * Définit la consigne manuelle.
 * POST /api/setpoint — CONTRACTS §2.4
 * @param {number} value - Consigne en °C (0.0 … 35.0)
 * @returns {Promise<object>} SetpointResponse { status, setpoint }
 */
export function setSetpoint(value) {
  return request('/api/setpoint', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ setpoint: value })
  });
}

/**
 * Récupère le lot (profil de température).
 * GET /api/profile — CONTRACTS_v0.4.0 §2.1
 * @returns {Promise<object>} BatchResponse
 */
export function getProfile() {
  return request('/api/profile');
}

/**
 * Enregistre le lot (remplacement complet).
 * POST /api/profile — CONTRACTS_v0.4.0 §2.2
 * @param {object} batch - BatchSaveRequest { name, steps }
 * @returns {Promise<object>} { status, step_count }
 */
export function saveProfile(batch) {
  return request('/api/profile', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(batch)
  });
}

/**
 * Démarre ou arrête le lot.
 * POST /api/profile/activate — CONTRACTS_v0.4.0 §2.3
 * @param {boolean} active - true pour démarrer, false pour arrêter
 * @returns {Promise<object>} BatchActivateResponse
 */
export function activateProfile(active) {
  return request('/api/profile/activate', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ active: active })
  });
}

/**
 * Redémarre le contrôleur.
 * POST /api/restart — CONTRACTS §2.10
 * @returns {Promise<object>} RestartResponse { status, delay_ms }
 */
export function restart() {
  return request('/api/restart', {
    method: 'POST'
  });
}
