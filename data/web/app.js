/**
 * FermCon v0.4.0 — Bootstrap, routeur d'onglets et cycle de vie
 *
 * Point d'entrée unique du frontend.
 * - Routeur d'onglets avec fragment d'URL (#dashboard, #regulation, #batch, ...)
 * - Menu burger responsive (< 640px)
 * - Polling de /api/status avec backoff exponentiel et suspension sur document.hidden
 * - Délégation vers views.js (renderDashboard) et forms.js (chargement dynamique)
 * - Bandeau d'erreur global (#globalError)
 * - Redirection de #profile et #fermentation vers #batch (v0.4.0)
 *
 * Un seul DOMContentLoaded dans tout le frontend (CONTRACTS §4.2).
 */

import { getStatus, restart, POLL_INTERVAL_MS, POLL_BACKOFF_MAX_MS, POLL_BACKOFF_FACTOR } from './api.js';

// ---------------------------------------------------------------------------
// État
// ---------------------------------------------------------------------------

/** Identifiant de l'onglet actif. */
let currentTab = 'dashboard';

/** Timer setTimeout du prochain poll. */
let pollTimer = null;

/** Verrou : true si une requête /api/status est en cours. */
let pollPending = false;

/** Intervalle courant du polling (ms), varie avec le backoff. */
let pollInterval = POLL_INTERVAL_MS;

/** Module views.js chargé dynamiquement, ou null. */
let viewsModule = null;

/** Module forms.js chargé dynamiquement, ou null. */
let formsModule = null;

// ---------------------------------------------------------------------------
// Mapping onglets → panneaux
// ---------------------------------------------------------------------------

/** Liste ordonnée des identifiants d'onglets (ordre du DOM). v0.4.0 : 5 onglets. */
const TAB_IDS = ['dashboard', 'regulation', 'batch', 'network', 'integrations'];

/**
 * Associe un identifiant d'onglet à l'id du panneau <section> correspondant.
 * @param {string} tabId
 * @returns {string} id du panneau
 */
function panelId(tabId) {
  if (tabId === 'batch') return 'panelBatch';
  return 'panel' + tabId.charAt(0).toUpperCase() + tabId.slice(1);
}

// ---------------------------------------------------------------------------
// Bandeau d'erreur global
// ---------------------------------------------------------------------------

function showGlobalError(message) {
  const el = document.getElementById('globalError');
  if (!el) return;
  el.textContent = message;
  el.className = 'global-error';
}

function hideGlobalError() {
  const el = document.getElementById('globalError');
  if (!el) return;
  el.textContent = '';
  el.className = 'global-error hidden';
}

// ---------------------------------------------------------------------------
// Routeur d'onglets
// ---------------------------------------------------------------------------

/**
 * Active l'onglet désigné.
 * - Met à jour les classes CSS des boutons et panneaux
 * - Reflète l'onglet dans le fragment d'URL
 * - Ferme le menu burger
 * - Déclenche le chargement des données spécifiques à l'onglet
 *
 * @param {string} tabId - Identifiant de l'onglet
 */
function switchTab(tabId) {
  // v0.4.0 : redirection des anciennes ancres
  if (tabId === 'profile' || tabId === 'fermentation') {
    tabId = 'batch';
  }

  if (!TAB_IDS.includes(tabId)) return;
  currentTab = tabId;

  // Mise à jour des boutons d'onglet
  document.querySelectorAll('.tab-btn').forEach(function (btn) {
    btn.classList.toggle('active', btn.dataset.tab === tabId);
  });

  // Mise à jour des panneaux
  document.querySelectorAll('.tab-panel').forEach(function (panel) {
    panel.classList.toggle('active', panel.id === panelId(tabId));
  });

  // Fragment d'URL (sans déclencher hashchange)
  if (window.location.hash !== '#' + tabId) {
    history.replaceState(null, '', '#' + tabId);
  }

  // Fermeture du menu burger
  var menuToggle = document.getElementById('menuToggle');
  if (menuToggle) menuToggle.checked = false;

  // Chargement des données spécifiques à l'onglet
  loadTabData(tabId);
}

/**
 * Charge les données nécessaires à l'onglet actif via forms.js.
 * Sans effet si forms.js n'est pas encore chargé.
 *
 * @param {string} tabId
 */
function loadTabData(tabId) {
  if (!formsModule) return;

  switch (tabId) {
    case 'regulation':
      if (typeof formsModule.loadRegulation === 'function') formsModule.loadRegulation();
      break;
    case 'batch':
      if (typeof formsModule.loadBatch === 'function') formsModule.loadBatch();
      break;
    case 'network':
      if (typeof formsModule.loadNetwork === 'function') formsModule.loadNetwork();
      break;
    case 'integrations':
      if (typeof formsModule.loadIntegrations === 'function') formsModule.loadIntegrations();
      break;
  }
}

/**
 * Restaure l'onglet depuis le fragment d'URL au chargement initial.
 * Si aucun fragment valide, active l'onglet Tableau de bord par défaut.
 * v0.4.0 : #profile et #fermentation redirigent vers #batch.
 */
function restoreTabFromHash() {
  var hash = window.location.hash.replace('#', '');
  if (hash === 'profile' || hash === 'fermentation') {
    hash = 'batch';
  }
  if (hash && TAB_IDS.includes(hash)) {
    switchTab(hash);
  } else {
    switchTab('dashboard');
  }
}

// ---------------------------------------------------------------------------
// Polling de /api/status avec backoff exponentiel
// ---------------------------------------------------------------------------

function schedulePoll() {
  if (pollTimer) clearTimeout(pollTimer);
  pollTimer = setTimeout(pollStatus, pollInterval);
}

async function pollStatus() {
  if (pollPending) {
    schedulePoll();
    return;
  }

  if (document.hidden) {
    return;
  }

  pollPending = true;

  try {
    var status = await getStatus();

    pollInterval = POLL_INTERVAL_MS;

    if (viewsModule && typeof viewsModule.renderDashboard === 'function') {
      try {
        viewsModule.renderDashboard(status);
      } catch (renderErr) {
        console.error('Erreur dans renderDashboard:', renderErr);
      }
    }

    hideGlobalError();
  } catch (error) {
    pollInterval = Math.min(pollInterval * POLL_BACKOFF_FACTOR, POLL_BACKOFF_MAX_MS);
    showGlobalError(error.message || 'Erreur de communication avec le contrôleur');
    console.error('Polling error:', error);
  } finally {
    pollPending = false;
    schedulePoll();
  }
}

// ---------------------------------------------------------------------------
// Chargement dynamique des modules
// ---------------------------------------------------------------------------

async function loadModules() {
  try {
    viewsModule = await import('./views.js');
  } catch (e) {
    console.warn('views.js non disponible');
  }

  try {
    formsModule = await import('./forms.js');
  } catch (e) {
    console.warn('forms.js non disponible');
  }
}

// ---------------------------------------------------------------------------
// Initialisation — unique DOMContentLoaded (CONTRACTS §4.2)
// ---------------------------------------------------------------------------

document.addEventListener('DOMContentLoaded', function () {

  loadModules().then(function () {

    // --- Écouteurs des onglets ---
    document.querySelectorAll('.tab-btn').forEach(function (btn) {
      btn.addEventListener('click', function () {
        var tabId = btn.dataset.tab;
        if (tabId) switchTab(tabId);
      });
    });

    // --- Restauration de l'onglet depuis le hash ---
    restoreTabFromHash();

    // --- Écouteur hashchange (navigation navigateur) ---
    window.addEventListener('hashchange', function () {
      var hash = window.location.hash.replace('#', '');
      if (hash === 'profile' || hash === 'fermentation') {
        hash = 'batch';
      }
      if (hash && TAB_IDS.includes(hash) && hash !== currentTab) {
        switchTab(hash);
      }
    });

    // --- Bouton de redémarrage ---
    var restartBtn = document.getElementById('netRestartBtn');
    if (restartBtn) {
      restartBtn.addEventListener('click', function () {
        if (!confirm('Redémarrer le contrôleur ? L\'interface sera momentanément indisponible.')) {
          return;
        }
        restartBtn.disabled = true;
        restart().then(function () {
          showGlobalError('Redémarrage en cours — l\'interface sera de retour dans quelques secondes.');
        }).catch(function (error) {
          showGlobalError(error.message || 'Échec du redémarrage');
          restartBtn.disabled = false;
        });
      });
    }

    // --- Suspension / reprise du polling sur changement de visibilité ---
    document.addEventListener('visibilitychange', function () {
      if (!document.hidden && !pollPending) {
        if (pollTimer) clearTimeout(pollTimer);
        pollInterval = POLL_INTERVAL_MS;
        pollStatus();
      }
    });

    // --- Démarrage du polling ---
    pollStatus();

    // --- Chargement initial de la configuration (pour forms.js) ---
    if (formsModule && typeof formsModule.loadConfig === 'function') {
      formsModule.loadConfig().catch(function (e) {
        console.warn('Échec du chargement initial de la configuration:', e);
      });
    }

  });

});
