/**
 * FermCon v0.4.0 — Formulaires (FE5)
 *
 * Quatre formulaires : Régulation, Lot, Réseau, Intégrations.
 * Les formulaires Profil et Fermentation (v0.3.0) sont fusionnés en un seul onglet Lot.
 *
 * Règles (CONTRACTS_v0.4.0, MODULES.md §FE5) :
 * - Bornes min/max/step dynamiques depuis bounds de GET /api/config
 * - Aucune borne codée en dur (sauf facteurs de conversion d'unité)
 * - Validation client avant tout appel réseau
 * - 400 VALIDATION_ERROR → surligner le champ nommé par field
 * - Contraintes C1 et C2 vérifiées côté client
 * - Mots de passe vides à l'affichage, envoi vide = ne pas changer
 * - reboot_required: true → proposer explicitement le redémarrage
 * - Consigne manuelle désactivée si drives_setpoint (ADR-011)
 * - Bug 4 corrigé : activation n'efface pas les étapes
 * - 409 TIME_NOT_SYNCED → message compréhensible
 * - Chaque bouton : état chargement, désactivé pendant requête, retour succès/échec
 * - Valeurs rechargées seulement à l'ouverture et après sauvegarde réussie
 * - ADR-013 : unité de durée = préférence d'interface, durée toujours en secondes dans le contrat
 * - Aucun innerHTML sur données API ; labels insérés par textContent
 */

import {
  getConfig, saveConfig, setSetpoint, getStatus,
  getProfile, saveProfile, activateProfile,
  restart
} from './api.js';

// ---------------------------------------------------------------------------
// État interne
// ---------------------------------------------------------------------------

/** Configuration courante (incluant bounds). null = pas encore chargée. */
let currentConfig = null;

/** Lot courant (BatchResponse). null = pas encore chargé. */
let currentBatch = null;

/** Indique si le lot a des modifications non enregistrées (Bug 4). */
let batchDirty = false;

/** Indique si le lot pilote la consigne (drives_setpoint, ADR-011). */
let batchDrivesSetpoint = false;

// ---------------------------------------------------------------------------
// Constantes (CONTRACTS_v0.4.0 §5)
// ---------------------------------------------------------------------------

/** Facteurs de conversion vers les secondes (ADR-013). */
const DURATION_UNITS = [
  { label: 'Minutes', value: 'minutes', factor: 60 },
  { label: 'Heures',   value: 'hours',   factor: 3600 },
  { label: 'Jours',    value: 'days',    factor: 86400 }
];

/** Unité proposée pour une nouvelle étape. */
const DEFAULT_DURATION_UNIT = 'hours';

/** Facteurs de conversion (accès rapide). */
const UNIT_TO_SECONDS = {
  minutes: 60,
  hours: 3600,
  days: 86400
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Affiche un message dans un conteneur de formulaire.
 * @param {string} elId - ID de l'élément conteneur
 * @param {string} text - Texte du message
 * @param {'success'|'error'|'loading'|'warning'|''} type - Type de message
 */
function showFormMsg(elId, text, type) {
  const el = document.getElementById(elId);
  if (!el) return;
  el.textContent = text;
  el.className = 'form-msg' + (type ? ' ' + type : '');
}

/**
 * Efface le message et la classe du conteneur.
 * @param {string} elId
 */
function clearFormMsg(elId) {
  showFormMsg(elId, '', '');
}

/**
 * Positionne les attributs min, max, step d'un champ depuis bounds.
 * @param {string} fieldId - ID de l'input
 * @param {object} bound - { min, max } depuis bounds
 * @param {number} [step] - Pas (défaut : calculé depuis min/max)
 */
function applyBounds(fieldId, bound, step) {
  const el = document.getElementById(fieldId);
  if (!el || !bound) return;

  el.min = bound.min;
  el.max = bound.max;

  if (step !== undefined) {
    el.step = step;
  } else {
    const isFloat = !Number.isInteger(bound.min) || !Number.isInteger(bound.max);
    el.step = isFloat ? '0.1' : '1';
  }
}

/**
 * Met à jour le span de range pour afficher les bornes.
 * @param {string} rangeId - ID du span de range
 * @param {object} bound - { min, max }
 * @param {string} [unit=''] - Unité
 */
function updateRangeDisplay(rangeId, bound, unit) {
  const el = document.getElementById(rangeId);
  if (!el) return;
  if (bound) {
    el.textContent = bound.min + (unit || '') + ' – ' + bound.max + (unit || '');
  }
}

/**
 * Active/désactive un bouton avec état de chargement.
 * @param {string} btnId - ID du bouton
 * @param {boolean} loading - true = chargement, false = normal
 * @param {string} [label] - Texte du bouton au repos
 */
function setBtnLoading(btnId, loading, label) {
  const btn = document.getElementById(btnId);
  if (!btn) return;
  btn.disabled = loading;
  if (loading) {
    btn.dataset.prevLabel = btn.textContent;
    btn.textContent = 'Enregistrement...';
  } else {
    btn.textContent = label || btn.dataset.prevLabel || btn.textContent;
  }
}

/**
 * Surligne un champ en erreur et affiche le message.
 * @param {string} fieldId - ID du champ (ou nom du champ)
 * @param {string} message - Message d'erreur
 * @param {number} [min] - Borne basse
 * @param {number} [max] - Borne haute
 */
function highlightField(fieldId, message, min, max) {
  document.querySelectorAll('.form-group input.field-error, .field-error').forEach(function (el) {
    el.classList.remove('field-error');
  });

  let el = document.getElementById(fieldId);
  if (!el) {
    el = document.querySelector('[name="' + fieldId + '"]');
  }

  if (el) {
    el.classList.add('field-error');
    el.scrollIntoView({ behavior: 'smooth', block: 'center' });
  }

  let fullMsg = message;
  if (min !== undefined && max !== undefined) {
    fullMsg += ' (' + min + ' – ' + max + ')';
  }
  return fullMsg;
}

/**
 * Valide qu'une valeur est dans les bornes.
 * @param {*} value - Valeur à tester
 * @param {object} bound - { min, max }
 * @returns {boolean}
 */
function inBounds(value, bound) {
  if (value === null || value === undefined || value === '' || (typeof value === 'number' && isNaN(value))) {
    return false;
  }
  const n = Number(value);
  return !isNaN(n) && n >= bound.min && n <= bound.max;
}

/**
 * Détermine si une valeur est "vide" (null, undefined, '', NaN).
 * @param {*} v
 * @returns {boolean}
 */
function isEmpty(v) {
  return v === null || v === undefined || v === '' || (typeof v === 'number' && isNaN(v));
}

// ---------------------------------------------------------------------------
// Conversion d'unités de durée (FE5 — ADR-013)
// ---------------------------------------------------------------------------

/**
 * Détermine l'unité la plus lisible pour une durée en secondes.
 * ADR-013 : la plus grande unité donnant une valeur entière.
 *   86400 s → "1" jour, 5400 s → "90" minutes, 3600 s → "1" heure.
 * @param {number} durationS - Durée en secondes
 * @returns {{ displayValue: string, unit: string }}
 */
function secondsToBestUnit(durationS) {
  if (durationS == null || isNaN(durationS) || durationS <= 0) {
    return { displayValue: '', unit: DEFAULT_DURATION_UNIT };
  }
  // Essayer jours (plus grande unité avec valeur entière)
  if (durationS % 86400 === 0) {
    return { displayValue: String(durationS / 86400), unit: 'days' };
  }
  // Essayer heures
  if (durationS % 3600 === 0) {
    return { displayValue: String(durationS / 3600), unit: 'hours' };
  }
  // Fallback : minutes (toujours entier car durationS est entier)
  return { displayValue: String(durationS / 60), unit: 'minutes' };
}

/**
 * Applique les attributs min/max/step d'un champ durée selon l'unité.
 * Bornes : 60 s (1 min) à 2592000 s (30 jours).
 * @param {HTMLInputElement} inpDur - Input de durée
 * @param {string} unit - 'minutes', 'hours', ou 'days'
 */
function applyDurationBounds(inpDur, unit) {
  if (!inpDur) return;
  if (unit === 'minutes') {
    inpDur.min = '1';
    inpDur.max = '43200';
    inpDur.step = '1';
  } else if (unit === 'hours') {
    inpDur.min = '1';
    inpDur.max = '720';
    inpDur.step = '1';
  } else {
    inpDur.min = '1';
    inpDur.max = '30';
    inpDur.step = '1';
  }
}

/**
 * Valide qu'une durée saisie dans une unité donnée correspond à un nombre
 * entier de secondes et respecte les bornes 60–2592000 s.
 * @param {number} displayValue - Valeur saisie
 * @param {string} unit - 'minutes', 'hours', ou 'days'
 * @returns {{ valid: boolean, seconds: number, message: string }}
 */
function validateDuration(displayValue, unit) {
  if (displayValue == null || isNaN(displayValue) || displayValue <= 0) {
    return { valid: false, seconds: 0, message: 'La durée doit être strictement positive' };
  }

  const factor = UNIT_TO_SECONDS[unit];
  if (!factor) {
    return { valid: false, seconds: 0, message: 'Unité inconnue' };
  }

  const rawSeconds = displayValue * factor;

  // Vérifier que la valeur tombe sur une seconde entière
  if (!Number.isInteger(rawSeconds)) {
    return {
      valid: false,
      seconds: 0,
      message: 'La durée ne correspond pas à un nombre entier de secondes (' +
        displayValue + ' ' + (unit === 'minutes' ? 'min' : unit === 'hours' ? 'h' : 'j') +
        ' = ' + rawSeconds.toFixed(2) + ' s). Ajustez la valeur.'
    };
  }

  const seconds = Math.round(rawSeconds);

  // Bornes 60–2592000 s
  if (seconds < 60) {
    return { valid: false, seconds: 0, message: 'La durée minimum est de 60 secondes (1 minute)' };
  }
  if (seconds > 2592000) {
    return { valid: false, seconds: 0, message: 'La durée maximum est de 2 592 000 secondes (30 jours)' };
  }

  return { valid: true, seconds: seconds, message: '' };
}

// ---------------------------------------------------------------------------
// Chargement initial de la configuration (appelé par app.js au démarrage)
// ---------------------------------------------------------------------------

/**
 * Charge la configuration au démarrage pour initialiser currentConfig.
 * Appelé par app.js dans DOMContentLoaded.
 * @returns {Promise<void>}
 */
export async function loadConfig() {
  try {
    currentConfig = await getConfig();
  } catch (e) {
    console.warn('Échec du chargement initial de la configuration:', e);
    currentConfig = null;
  }
}


// ---------------------------------------------------------------------------
// 1. Formulaire Régulation
// ---------------------------------------------------------------------------

/**
 * Charge et peuple le formulaire de régulation.
 * Appelé par app.js à l'ouverture de l'onglet Régulation.
 */
export async function loadRegulation() {
  clearFormMsg('regFormMsg');

  try {
    currentConfig = await getConfig();
  } catch (e) {
    showFormMsg('regFormMsg', 'Erreur de chargement : ' + (e.message || 'Impossible de charger la configuration'), 'error');
    return;
  }

  if (!currentConfig) {
    showFormMsg('regFormMsg', 'Configuration non disponible', 'error');
    return;
  }

  // Récupérer l'état du lot pour savoir si drives_setpoint est vrai (ADR-011)
  try {
    const profile = await getProfile();
    batchDrivesSetpoint = profile.drives_setpoint === true;
  } catch (e) {
    batchDrivesSetpoint = false;
  }

  const cfg = currentConfig;
  const b = cfg.bounds || {};

  // Peupler les champs et appliquer les bornes dynamiques
  const fields = [
    { id: 'regSetpoint',           key: 'setpoint',              bound: b.setpoint,              rangeId: 'regSetpointRange',           unit: ' °C' },
    { id: 'regHysteresis',         key: 'hysteresis',            bound: b.hysteresis,            rangeId: 'regHysteresisRange',         unit: ' °C' },
    { id: 'regTempOffset',         key: 'temp_offset',           bound: b.temp_offset,           rangeId: 'regTempOffsetRange',         unit: ' °C' },
    { id: 'regMinCompressorDelay', key: 'min_compressor_delay',  bound: b.min_compressor_delay,  rangeId: 'regMinCompressorDelayRange',  unit: ' s' },
    { id: 'regCoolMinOnS',         key: 'cool_min_on_s',         bound: b.cool_min_on_s,         rangeId: 'regCoolMinOnSRange',         unit: ' s' },
    { id: 'regHeatMinOnS',         key: 'heat_min_on_s',         bound: b.heat_min_on_s,         rangeId: 'regHeatMinOnSRange',         unit: ' s' },
    { id: 'regMaxOnTimeoutS',      key: 'max_on_timeout_s',      bound: b.max_on_timeout_s,      rangeId: 'regMaxOnTimeoutSRange',      unit: ' s' },
    { id: 'regTempReadIntervalMs', key: 'temp_read_interval_ms', bound: b.temp_read_interval_ms, rangeId: 'regTempReadIntervalMsRange', unit: ' ms' },
    { id: 'regTempPlausibleMinC',  key: 'temp_plausible_min_c',  bound: b.temp_plausible_min_c,  rangeId: 'regTempPlausibleMinCRange',  unit: ' °C' },
    { id: 'regTempPlausibleMaxC',  key: 'temp_plausible_max_c',  bound: b.temp_plausible_max_c,  rangeId: 'regTempPlausibleMaxCRange',  unit: ' °C' },
    { id: 'regTempFaultTripS',     key: 'temp_fault_trip_s',     bound: b.temp_fault_trip_s,     rangeId: 'regTempFaultTripSRange',     unit: ' s' },
    { id: 'regTempFaultClearS',    key: 'temp_fault_clear_s',    bound: b.temp_fault_clear_s,    rangeId: 'regTempFaultClearSRange',    unit: ' s' }
  ];

  fields.forEach(function (f) {
    const el = document.getElementById(f.id);
    if (!el) return;

    const val = cfg[f.key];
    el.value = (val !== null && val !== undefined) ? val : '';

    if (f.bound) {
      applyBounds(f.id, f.bound);
      updateRangeDisplay(f.rangeId, f.bound, f.unit);
    } else {
      updateRangeDisplay(f.rangeId, null);
    }

    el.classList.remove('field-error');
  });

  // Gérer la consigne manuelle (ADR-011 : désactivée si drives_setpoint)
  updateSetpointState();

  // Écouteur de soumission (une seule fois)
  setupRegulationSubmit();
}

/** Flag pour éviter les écouteurs dupliqués. */
let regSubmitBound = false;

function setupRegulationSubmit() {
  if (regSubmitBound) return;
  regSubmitBound = true;

  const form = document.getElementById('regForm');
  if (!form) return;

  form.addEventListener('submit', async function (e) {
    e.preventDefault();
    clearFormMsg('regFormMsg');

    document.querySelectorAll('#regForm .field-error').forEach(function (el) {
      el.classList.remove('field-error');
    });

    const cfg = currentConfig;
    if (!cfg) {
      showFormMsg('regFormMsg', 'Configuration non chargée', 'error');
      return;
    }
    const b = cfg.bounds || {};

    const partial = {};
    const fieldDefs = [
      { id: 'regSetpoint',           key: 'setpoint',              bound: b.setpoint },
      { id: 'regHysteresis',         key: 'hysteresis',            bound: b.hysteresis },
      { id: 'regTempOffset',         key: 'temp_offset',           bound: b.temp_offset },
      { id: 'regMinCompressorDelay', key: 'min_compressor_delay',  bound: b.min_compressor_delay },
      { id: 'regCoolMinOnS',         key: 'cool_min_on_s',         bound: b.cool_min_on_s },
      { id: 'regHeatMinOnS',         key: 'heat_min_on_s',         bound: b.heat_min_on_s },
      { id: 'regMaxOnTimeoutS',      key: 'max_on_timeout_s',      bound: b.max_on_timeout_s },
      { id: 'regTempReadIntervalMs', key: 'temp_read_interval_ms', bound: b.temp_read_interval_ms },
      { id: 'regTempPlausibleMinC',  key: 'temp_plausible_min_c',  bound: b.temp_plausible_min_c },
      { id: 'regTempPlausibleMaxC',  key: 'temp_plausible_max_c',  bound: b.temp_plausible_max_c },
      { id: 'regTempFaultTripS',     key: 'temp_fault_trip_s',     bound: b.temp_fault_trip_s },
      { id: 'regTempFaultClearS',    key: 'temp_fault_clear_s',    bound: b.temp_fault_clear_s }
    ];

    let hasError = false;
    const collected = {};
    for (let i = 0; i < fieldDefs.length; i++) {
      const f = fieldDefs[i];
      const el = document.getElementById(f.id);
      if (!el) continue;
      const raw = el.value;
      if (raw === '') continue;

      const num = Number(raw);
      if (isNaN(num)) {
        highlightField(f.id, 'Valeur numérique requise pour ' + f.key);
        showFormMsg('regFormMsg', 'Valeur numérique requise pour ' + f.key, 'error');
        hasError = true;
        break;
      }

      if (f.bound && !inBounds(num, f.bound)) {
        const msg = highlightField(f.id, f.key + ' hors bornes', f.bound.min, f.bound.max);
        showFormMsg('regFormMsg', msg, 'error');
        hasError = true;
        break;
      }

      collected[f.key] = num;
    }

    if (hasError) return;

    if (Object.keys(collected).length === 0) {
      showFormMsg('regFormMsg', 'Aucune modification à enregistrer', '');
      return;
    }

    Object.assign(partial, collected);

    // Contrainte C1 : temp_plausible_max_c >= temp_plausible_min_c + 10
    const plausMin = collected.temp_plausible_min_c !== undefined ? collected.temp_plausible_min_c : cfg.temp_plausible_min_c;
    const plausMax = collected.temp_plausible_max_c !== undefined ? collected.temp_plausible_max_c : cfg.temp_plausible_max_c;
    if (plausMax < plausMin + 10) {
      const msg = highlightField('regTempPlausibleMaxC',
        'La plage plausible max doit être ≥ min + 10 °C (actuellement ' + plausMax.toFixed(1) + ' < ' + (plausMin + 10).toFixed(1) + ')');
      showFormMsg('regFormMsg', msg, 'error');
      return;
    }

    // Contrainte C2 : temp_fault_clear_s >= temp_fault_trip_s
    const trip = collected.temp_fault_trip_s !== undefined ? collected.temp_fault_trip_s : cfg.temp_fault_trip_s;
    const clear = collected.temp_fault_clear_s !== undefined ? collected.temp_fault_clear_s : cfg.temp_fault_clear_s;
    if (clear < trip) {
      const msg = highlightField('regTempFaultClearS',
        'Le délai de reprise (' + clear + ' s) doit être ≥ déclenchement (' + trip + ' s)');
      showFormMsg('regFormMsg', msg, 'error');
      return;
    }

    setBtnLoading('regSaveBtn', true);
    showFormMsg('regFormMsg', 'Enregistrement en cours...', 'loading');

    try {
      const result = await saveConfig(partial);
      setBtnLoading('regSaveBtn', false, 'Enregistrer');

      if (result.reboot_required) {
        showFormMsg('regFormMsg', 'Configuration enregistrée. Un redémarrage est requis pour appliquer les changements.', 'success');
        proposeRestart('regFormMsg');
      } else {
        showFormMsg('regFormMsg', 'Configuration enregistrée avec succès', 'success');
      }

      currentConfig = await getConfig();
    } catch (error) {
      setBtnLoading('regSaveBtn', false, 'Enregistrer');

      if (error.code === 'VALIDATION_ERROR' && error.field) {
        const msg = highlightField(error.field, error.message, error.min, error.max);
        showFormMsg('regFormMsg', msg, 'error');
      } else {
        showFormMsg('regFormMsg', error.message || 'Erreur lors de l\'enregistrement', 'error');
      }
    }
  });
}

/**
 * Désactive la consigne manuelle si le lot pilote la consigne (ADR-011).
 * Active le hint explicatif.
 */
function updateSetpointState() {
  const el = document.getElementById('regSetpoint');
  const hintEl = document.getElementById('regSetpointHint');
  if (!el) return;

  if (batchDrivesSetpoint) {
    el.disabled = true;
    el.title = 'Consigne pilotée par le lot actif — arrêtez le lot pour modifier manuellement';
    if (hintEl) {
      hintEl.textContent = 'La consigne est pilotée par le lot actif. Arrêtez le lot dans l\'onglet Lot pour reprendre le contrôle manuel.';
      hintEl.classList.remove('hidden');
    }
  } else {
    el.disabled = false;
    el.title = '';
    if (hintEl) {
      hintEl.textContent = '';
      hintEl.classList.add('hidden');
    }
  }
}


// ---------------------------------------------------------------------------
// 2. Formulaire Lot (fusion Profil + Fermentation, v0.4.0)
// ---------------------------------------------------------------------------

let batchListenersBound = false;

/**
 * Charge et peuple le formulaire Lot.
 * Appelé par app.js à l'ouverture de l'onglet Lot.
 */
export async function loadBatch() {
  clearFormMsg('batchFormMsg');

  try {
    currentBatch = await getProfile();
  } catch (e) {
    showFormMsg('batchFormMsg', 'Erreur de chargement : ' + (e.message || 'Impossible de charger le lot'), 'error');
    return;
  }

  if (!currentBatch) {
    showFormMsg('batchFormMsg', 'Lot non disponible', 'error');
    return;
  }

  const p = currentBatch;

  // Nom
  const nameEl = document.getElementById('batchName');
  if (nameEl) nameEl.value = p.name || '';

  // État
  batchDrivesSetpoint = p.drives_setpoint === true;
  updateBatchButtons(p.active === true);
  updateSetpointState();

  // Reconstruire les étapes
  renderBatchSteps(p.steps || []);
  batchDirty = false;

  // Message si lot sans étape
  updateEmptyBatchHint(p.steps || [], p.active === true);

  // Écouteurs (une seule fois)
  if (!batchListenersBound) {
    batchListenersBound = true;
    setupBatchListeners();
  }
}

function setupBatchListeners() {
  const addBtn = document.getElementById('batchAddStepBtn');
  const saveBtn = document.getElementById('batchSaveBtn');
  const startBtn = document.getElementById('batchStartBtn');
  const stopBtn = document.getElementById('batchStopBtn');

  if (addBtn) {
    addBtn.addEventListener('click', function () {
      addBatchStep();
    });
  }

  if (saveBtn) {
    saveBtn.addEventListener('click', async function () {
      await handleBatchSave();
    });
  }

  if (startBtn) {
    startBtn.addEventListener('click', async function () {
      await handleBatchActivate(true);
    });
  }

  if (stopBtn) {
    stopBtn.addEventListener('click', async function () {
      await handleBatchActivate(false);
    });
  }
}

/**
 * Affiche ou masque un message indiquant que le lot n'a pas d'étape.
 * @param {Array} steps
 * @param {boolean} active
 */
function updateEmptyBatchHint(steps, active) {
  const msgEl = document.getElementById('batchFormMsg');
  if (!steps || steps.length === 0) {
    if (active) {
      showFormMsg('batchFormMsg',
        'Ce lot est actif sans étape. Les jours sont comptés mais aucune consigne de température n\'est appliquée — la régulation suit la consigne manuelle.',
        '');
    } else {
      showFormMsg('batchFormMsg',
        'Ce lot n\'a pas d\'étape. Vous pouvez l\'enregistrer et le démarrer tel quel : les jours seront comptés et la régulation suivra la consigne manuelle.',
        '');
    }
  }
}

/**
 * Rend les étapes du lot dans le tableau.
 * Colonnes : Type, Début (°C), Fin (°C), Durée, Unité, Nom, Actions
 * @param {Array} steps - Tableau de BatchStep
 */
function renderBatchSteps(steps) {
  const tbody = document.getElementById('batchStepsTbody');
  if (!tbody) return;

  // Nettoyer en préservant les écouteurs (remplacement complet)
  while (tbody.firstChild) {
    tbody.removeChild(tbody.firstChild);
  }

  if (!steps || steps.length === 0) {
    const tr = document.createElement('tr');
    const td = document.createElement('td');
    td.colSpan = 7;
    td.style.color = 'var(--color-text-dim)';
    td.style.fontStyle = 'italic';
    td.textContent = 'Aucune étape — ajoutez une étape pour définir un profil de température';
    tr.appendChild(td);
    tbody.appendChild(tr);
    return;
  }

  steps.forEach(function (step, idx) {
    const tr = document.createElement('tr');
    tr.dataset.index = idx;

    // --- Type ---
    const tdType = document.createElement('td');
    const selType = document.createElement('select');
    selType.className = 'step-type';
    selType.innerHTML = '<option value="PALIER">PALIER</option><option value="RAMPE">RAMPE</option>';
    selType.value = step.type || 'PALIER';
    selType.addEventListener('change', function () {
      markBatchDirty();
      updateStepRow(tr);
    });
    tdType.appendChild(selType);
    tr.appendChild(tdType);

    // --- Début (°C) ---
    const tdStart = document.createElement('td');
    const inpStart = document.createElement('input');
    inpStart.type = 'number';
    inpStart.className = 'step-tempstart';
    inpStart.step = '0.1';
    inpStart.min = '0';
    inpStart.max = '35';
    inpStart.value = step.tempStart != null ? step.tempStart : '';
    inpStart.addEventListener('input', function () { markBatchDirty(); });
    tdStart.appendChild(inpStart);
    tr.appendChild(tdStart);

    // --- Fin (°C) (visible seulement pour RAMPE) ---
    const tdEnd = document.createElement('td');
    const inpEnd = document.createElement('input');
    inpEnd.type = 'number';
    inpEnd.className = 'step-tempend';
    inpEnd.step = '0.1';
    inpEnd.min = '0';
    inpEnd.max = '35';
    inpEnd.value = step.tempEnd != null ? step.tempEnd : '';
    inpEnd.addEventListener('input', function () { markBatchDirty(); });
    tdEnd.appendChild(inpEnd);
    tr.appendChild(tdEnd);

    // --- Durée + Unité ---
    const tdDur = document.createElement('td');
    const durContainer = document.createElement('div');
    durContainer.style.display = 'flex';
    durContainer.style.gap = '4px';
    durContainer.style.alignItems = 'center';

    const inpDur = document.createElement('input');
    inpDur.type = 'number';
    inpDur.className = 'step-duration';
    inpDur.style.flex = '1';
    inpDur.style.minWidth = '50px';

    const best = secondsToBestUnit(step.durationS);
    inpDur.value = best.displayValue;
    applyDurationBounds(inpDur, best.unit);
    inpDur.addEventListener('input', function () { markBatchDirty(); });

    const selUnit = document.createElement('select');
    selUnit.className = 'step-unit';
    DURATION_UNITS.forEach(function (u) {
      const opt = document.createElement('option');
      opt.value = u.value;
      opt.textContent = u.label;
      selUnit.appendChild(opt);
    });
    selUnit.value = best.unit;
    selUnit.dataset.prevUnit = best.unit;

    selUnit.addEventListener('change', function () {
      const oldUnit = selUnit.dataset.prevUnit || 'minutes';
      const oldValue = parseFloat(inpDur.value);
      if (!isNaN(oldValue) && oldValue > 0) {
        const seconds = oldValue * UNIT_TO_SECONDS[oldUnit];
        const newUnit = selUnit.value;
        const newValue = seconds / UNIT_TO_SECONDS[newUnit];
        if (newUnit === 'minutes') {
          inpDur.value = Math.round(newValue);
        } else {
          inpDur.value = parseFloat(newValue.toFixed(2));
        }
      }
      selUnit.dataset.prevUnit = selUnit.value;
      applyDurationBounds(inpDur, selUnit.value);
      markBatchDirty();
    });

    durContainer.appendChild(inpDur);
    durContainer.appendChild(selUnit);
    tdDur.appendChild(durContainer);
    tr.appendChild(tdDur);

    // --- Nom (label, 23 car. max, facultatif) ---
    const tdLabel = document.createElement('td');
    const inpLabel = document.createElement('input');
    inpLabel.type = 'text';
    inpLabel.className = 'step-label';
    inpLabel.maxLength = 23;
    inpLabel.placeholder = 'Ex: Primaire';
    inpLabel.value = step.label || '';
    inpLabel.addEventListener('input', function () { markBatchDirty(); });
    tdLabel.appendChild(inpLabel);
    tr.appendChild(tdLabel);

    // --- Actions ---
    const tdAct = document.createElement('td');
    const btnUp = document.createElement('button');
    btnUp.type = 'button';
    btnUp.className = 'btn btn-secondary';
    btnUp.textContent = '↑';
    btnUp.title = 'Remonter';
    btnUp.addEventListener('click', function () {
      moveBatchStep(idx, -1);
    });

    const btnDown = document.createElement('button');
    btnDown.type = 'button';
    btnDown.className = 'btn btn-secondary';
    btnDown.textContent = '↓';
    btnDown.title = 'Descendre';
    btnDown.addEventListener('click', function () {
      moveBatchStep(idx, 1);
    });

    const btnDel = document.createElement('button');
    btnDel.type = 'button';
    btnDel.className = 'btn btn-danger';
    btnDel.textContent = '✕';
    btnDel.title = 'Supprimer';
    btnDel.addEventListener('click', function () {
      removeBatchStep(idx);
    });

    tdAct.appendChild(btnUp);
    tdAct.appendChild(btnDown);
    tdAct.appendChild(btnDel);
    tr.appendChild(tdAct);

    tbody.appendChild(tr);

    // Mettre à jour l'affichage tempEnd selon le type
    updateStepRow(tr);
  });
}

/**
 * Met à jour l'affichage d'une ligne d'étape (visibilité tempEnd).
 * @param {HTMLTableRowElement} tr
 */
function updateStepRow(tr) {
  const selType = tr.querySelector('.step-type');
  const inpEnd = tr.querySelector('.step-tempend');
  if (!selType || !inpEnd) return;

  if (selType.value === 'PALIER') {
    inpEnd.parentElement.style.visibility = 'hidden';
    inpEnd.required = false;
  } else {
    inpEnd.parentElement.style.visibility = 'visible';
    inpEnd.required = true;
  }
}

/**
 * Ajoute une nouvelle étape PALIER par défaut.
 * ADR-011 : avertissement si première étape d'un lot déjà actif.
 */
function addBatchStep() {
  const tbody = document.getElementById('batchStepsTbody');
  if (!tbody) return;

  const existingRows = tbody.querySelectorAll('tr[data-index]');
  if (existingRows.length >= 16) {
    showFormMsg('batchFormMsg', 'Maximum 16 étapes atteint', 'error');
    return;
  }

  // ADR-011 : avertissement si première étape d'un lot actif
  const isFirstStep = existingRows.length === 0;
  const isActive = currentBatch && currentBatch.active === true;

  if (isFirstStep && isActive) {
    if (!confirm(
      'Attention : vous ajoutez une première étape à un lot déjà actif.\n\n' +
      'La consigne de température va basculer du mode manuel vers le profil de température défini par les étapes. ' +
      'Le compresseur et la résistance seront pilotés automatiquement.\n\n' +
      'Confirmez-vous l\'ajout de cette étape ?'
    )) {
      return;
    }
  }

  // Supprimer le message "aucune étape"
  const placeholder = tbody.querySelector('tr:not([data-index])');
  if (placeholder) placeholder.remove();

  const newStep = { label: '', type: 'PALIER', tempStart: 18.0, tempEnd: 18.0, durationS: 3600 };
  const steps = collectBatchStepsFromDOM();
  steps.push(newStep);
  renderBatchSteps(steps);
  markBatchDirty();

  if (isFirstStep) {
    showFormMsg('batchFormMsg', 'Première étape ajoutée — n\'oubliez pas d\'enregistrer', '');
  } else {
    showFormMsg('batchFormMsg', 'Étape ajoutée — n\'oubliez pas d\'enregistrer', '');
  }
}

/**
 * Supprime une étape.
 * @param {number} idx
 */
function removeBatchStep(idx) {
  const steps = collectBatchStepsFromDOM();
  if (idx < 0 || idx >= steps.length) return;
  steps.splice(idx, 1);
  renderBatchSteps(steps);
  markBatchDirty();

  // Réafficher le message si plus d'étape
  if (steps.length === 0) {
    updateEmptyBatchHint([], currentBatch ? currentBatch.active === true : false);
  }
}

/**
 * Déplace une étape.
 * @param {number} idx
 * @param {number} delta - -1 pour monter, +1 pour descendre
 */
function moveBatchStep(idx, delta) {
  const steps = collectBatchStepsFromDOM();
  const newIdx = idx + delta;
  if (newIdx < 0 || newIdx >= steps.length) return;

  const tmp = steps[idx];
  steps[idx] = steps[newIdx];
  steps[newIdx] = tmp;

  renderBatchSteps(steps);
  markBatchDirty();
}

/**
 * Collecte les étapes depuis le DOM.
 * Valide la conversion en secondes entières (ADR-013).
 * @returns {Array} Tableau de BatchStep
 */
function collectBatchStepsFromDOM() {
  const tbody = document.getElementById('batchStepsTbody');
  if (!tbody) return [];

  const rows = tbody.querySelectorAll('tr[data-index]');
  const steps = [];

  rows.forEach(function (tr) {
    const selType = tr.querySelector('.step-type');
    const inpStart = tr.querySelector('.step-tempstart');
    const inpEnd = tr.querySelector('.step-tempend');
    const inpDur = tr.querySelector('.step-duration');
    const selUnit = tr.querySelector('.step-unit');
    const inpLabel = tr.querySelector('.step-label');

    const type = selType ? selType.value : 'PALIER';
    const tempStart = inpStart ? Number(inpStart.value) : 0;
    const tempEnd = inpEnd ? Number(inpEnd.value) : tempStart;

    const durValue = inpDur ? parseFloat(inpDur.value) : 0;
    const unit = selUnit ? selUnit.value : 'minutes';
    const durationS = isNaN(durValue) || durValue <= 0 ? 0 : Math.round(durValue * UNIT_TO_SECONDS[unit]);

    const label = inpLabel ? inpLabel.value.trim() : '';

    steps.push({
      label: label,
      type: type,
      tempStart: isNaN(tempStart) ? 0 : tempStart,
      tempEnd: isNaN(tempEnd) ? tempStart : tempEnd,
      durationS: isNaN(durationS) ? 0 : durationS
    });
  });

  return steps;
}

/**
 * Marque le lot comme modifié (non enregistré).
 */
function markBatchDirty() {
  batchDirty = true;
}

/**
 * Met à jour les boutons Démarrer/Arrêter le lot.
 * @param {boolean} active
 */
function updateBatchButtons(active) {
  const startBtn = document.getElementById('batchStartBtn');
  const stopBtn = document.getElementById('batchStopBtn');

  if (startBtn) startBtn.style.display = active ? 'none' : '';
  if (stopBtn) stopBtn.style.display = active ? '' : 'none';
}

/**
 * Gère l'enregistrement du lot.
 */
async function handleBatchSave() {
  clearFormMsg('batchFormMsg');

  const nameEl = document.getElementById('batchName');
  const name = nameEl ? nameEl.value.trim() : '';

  if (!name || name.length < 1 || name.length > 40) {
    showFormMsg('batchFormMsg', 'Le nom du lot est requis (1 à 40 caractères)', 'error');
    return;
  }

  const steps = collectBatchStepsFromDOM();

  // Validation des étapes
  for (let i = 0; i < steps.length; i++) {
    const s = steps[i];

    // Validation du label
    if (s.label && s.label.length > 23) {
      showFormMsg('batchFormMsg', 'Étape ' + (i + 1) + ' : le nom ne doit pas dépasser 23 caractères', 'error');
      return;
    }

    if (s.tempStart < 0 || s.tempStart > 35) {
      showFormMsg('batchFormMsg', 'Étape ' + (i + 1) + ' : température de début hors bornes (0–35 °C)', 'error');
      return;
    }

    if (s.type === 'RAMPE' && (s.tempEnd < 0 || s.tempEnd > 35)) {
      showFormMsg('batchFormMsg', 'Étape ' + (i + 1) + ' : température de fin hors bornes (0–35 °C)', 'error');
      return;
    }

    // Validation de la durée (ADR-013 : secondes entières, bornes 60–2592000)
    if (!s.durationS || s.durationS <= 0) {
      showFormMsg('batchFormMsg', 'Étape ' + (i + 1) + ' : la durée doit être strictement positive', 'error');
      return;
    }
    if (s.durationS < 60) {
      showFormMsg('batchFormMsg', 'Étape ' + (i + 1) + ' : durée minimum 60 secondes (1 minute)', 'error');
      return;
    }
    if (s.durationS > 2592000) {
      showFormMsg('batchFormMsg', 'Étape ' + (i + 1) + ' : durée maximum 2 592 000 secondes (30 jours)', 'error');
      return;
    }
  }

  // Vérifier les secondes entières pour chaque étape (validation supplémentaire)
  for (let i = 0; i < steps.length; i++) {
    const tr = document.getElementById('batchStepsTbody').querySelectorAll('tr[data-index]')[i];
    if (!tr) continue;
    const inpDur = tr.querySelector('.step-duration');
    const selUnit = tr.querySelector('.step-unit');
    if (!inpDur || !selUnit) continue;

    const durValue = parseFloat(inpDur.value);
    const unit = selUnit.value;
    const validation = validateDuration(durValue, unit);
    if (!validation.valid) {
      showFormMsg('batchFormMsg', 'Étape ' + (i + 1) + ' : ' + validation.message, 'error');
      return;
    }
  }

  setBtnLoading('batchSaveBtn', true);
  showFormMsg('batchFormMsg', 'Enregistrement en cours...', 'loading');

  try {
    const result = await saveProfile({ name: name, steps: steps });
    setBtnLoading('batchSaveBtn', false, 'Enregistrer le lot');
    showFormMsg('batchFormMsg', 'Lot enregistré (' + result.step_count + ' étape(s))', 'success');
    batchDirty = false;

    // Recharger après sauvegarde
    currentBatch = await getProfile();
    batchDrivesSetpoint = currentBatch.drives_setpoint === true;
    updateBatchButtons(currentBatch.active === true);
    updateSetpointState();
    updateEmptyBatchHint(currentBatch.steps || [], currentBatch.active === true);
  } catch (error) {
    setBtnLoading('batchSaveBtn', false, 'Enregistrer le lot');

    if (error.code === 'VALIDATION_ERROR' && error.field) {
      showFormMsg('batchFormMsg', error.field + ' : ' + error.message, 'error');
    } else {
      showFormMsg('batchFormMsg', error.message || 'Erreur lors de l\'enregistrement', 'error');
    }
  }
}

/**
 * Gère le démarrage/arrêt du lot.
 * Bug 4 corrigé : ne réinitialise pas les étapes.
 * @param {boolean} activate - true = démarrer, false = arrêter
 */
async function handleBatchActivate(activate) {
  clearFormMsg('batchFormMsg');

  // Bug 4 : avertir si modifications non enregistrées avant démarrage
  if (activate && batchDirty) {
    if (!confirm('Des modifications du lot n\'ont pas été enregistrées. Voulez-vous les enregistrer avant de démarrer ?')) {
      return;
    }
    await handleBatchSave();
    if (batchDirty) return; // Échec de la sauvegarde
  }

  const btnId = activate ? 'batchStartBtn' : 'batchStopBtn';
  setBtnLoading(btnId, true);
  showFormMsg('batchFormMsg', activate ? 'Démarrage en cours...' : 'Arrêt en cours...', 'loading');

  try {
    const result = await activateProfile(activate);
    setBtnLoading('batchStartBtn', false, 'Démarrer le lot');
    setBtnLoading('batchStopBtn', false, 'Arrêter le lot');

    batchDrivesSetpoint = result.drives_setpoint === true;
    updateBatchButtons(result.active === true);
    updateSetpointState();

    showFormMsg('batchFormMsg',
      activate ? 'Lot démarré (consigne : ' + result.setpoint.toFixed(1) + ' °C)' : 'Lot arrêté',
      'success');

    // Recharger le lot après activation
    currentBatch = await getProfile();
    batchDrivesSetpoint = currentBatch.drives_setpoint === true;
    updateBatchButtons(currentBatch.active === true);
    updateSetpointState();
    updateEmptyBatchHint(currentBatch.steps || [], currentBatch.active === true);
  } catch (error) {
    setBtnLoading('batchStartBtn', false, 'Démarrer le lot');
    setBtnLoading('batchStopBtn', false, 'Arrêter le lot');

    if (error.code === 'TIME_NOT_SYNCED') {
      showFormMsg('batchFormMsg',
        'L\'horloge du contrôleur n\'est pas synchronisée (NTP). Le démarrage d\'un lot nécessite une heure valide. Vérifiez la connexion réseau et patientez quelques minutes.',
        'error');
    } else {
      showFormMsg('batchFormMsg', error.message || 'Erreur lors de l\'opération', 'error');
    }
  }
}


// ---------------------------------------------------------------------------
// 3. Formulaire Réseau
// ---------------------------------------------------------------------------

let netListenersBound = false;

/**
 * Charge et peuple le formulaire réseau.
 * Appelé par app.js à l'ouverture de l'onglet Réseau.
 */
export async function loadNetwork() {
  clearFormMsg('netFormMsg');

  let cfg;
  try {
    cfg = await getConfig();
    currentConfig = cfg;
  } catch (e) {
    showFormMsg('netFormMsg', 'Erreur de chargement : ' + (e.message || 'Impossible de charger la configuration réseau'), 'error');
    return;
  }

  if (!cfg) {
    showFormMsg('netFormMsg', 'Configuration non disponible', 'error');
    return;
  }

  // État
  const staEl = document.getElementById('netStaStatus');
  const apEl = document.getElementById('netApStatus');
  try {
    const status = await getStatus();
    if (staEl) {
      staEl.textContent = status.sta_connected ? 'Connecté' : 'Déconnecté';
      staEl.className = status.sta_connected ? 'data-value indicator-ok' : 'data-value indicator-warn';
    }
    if (apEl) {
      apEl.textContent = status.ap_clients > 0 ? 'Actif (' + status.ap_clients + ' client(s))' : 'En attente';
    }
  } catch (e) {
    if (staEl) staEl.textContent = '--';
    if (apEl) apEl.textContent = '--';
  }

  // WiFi
  const ssidEl = document.getElementById('netWifiSsid');
  const pwdEl = document.getElementById('netWifiPassword');
  const pwdSetEl = document.getElementById('netWifiPasswordSet');

  if (ssidEl) ssidEl.value = cfg.wifi_ssid || '';
  if (pwdEl) pwdEl.value = '';
  if (pwdSetEl) pwdSetEl.textContent = cfg.wifi_password_set ? '(défini)' : '(non défini)';

  // AP
  const apEnabledEl = document.getElementById('netApEnabled');
  const apSsidEl = document.getElementById('netApSsid');
  const apPwdEl = document.getElementById('netApPassword');
  const apPwdSetEl = document.getElementById('netApPasswordSet');

  if (apEnabledEl) apEnabledEl.checked = cfg.ap_enabled === true;
  if (apSsidEl) apSsidEl.value = cfg.ap_ssid || '';
  if (apPwdEl) apPwdEl.value = '';
  if (apPwdSetEl) apPwdSetEl.textContent = cfg.ap_password_set ? '(défini)' : '(non défini)';

  // Accès à l'interface
  const accessPwdEl = document.getElementById('netAccessPassword');
  const accessPwdSetEl = document.getElementById('netAccessPasswordSet');
  const accessPwdConfirmEl = document.getElementById('netAccessPasswordConfirm');

  if (accessPwdEl) accessPwdEl.value = '';
  if (accessPwdSetEl) accessPwdSetEl.textContent = cfg.password_set ? '(défini)' : '(non défini)';
  if (accessPwdConfirmEl) accessPwdConfirmEl.value = '';

  // Réinitialiser les erreurs
  document.querySelectorAll('#panelNetwork .field-error').forEach(function (el) {
    el.classList.remove('field-error');
  });

  // Écouteurs (une seule fois)
  if (!netListenersBound) {
    netListenersBound = true;
    setupNetworkListeners();
  }
}

function setupNetworkListeners() {
  const saveBtn = document.getElementById('netSaveBtn');

  if (saveBtn) {
    saveBtn.addEventListener('click', async function () {
      await handleNetworkSave();
    });
  }
}

async function handleNetworkSave() {
  clearFormMsg('netFormMsg');

  document.querySelectorAll('#panelNetwork .field-error').forEach(function (el) {
    el.classList.remove('field-error');
  });

  const partial = {};

  // WiFi SSID
  const ssidEl = document.getElementById('netWifiSsid');
  if (ssidEl && ssidEl.value !== (currentConfig ? currentConfig.wifi_ssid || '' : '')) {
    partial.wifi_ssid = ssidEl.value;
  }

  // WiFi password
  const pwdEl = document.getElementById('netWifiPassword');
  if (pwdEl && pwdEl.value !== '') {
    partial.wifi_password = pwdEl.value;
  }

  // AP enabled
  const apEnabledEl = document.getElementById('netApEnabled');
  const apEnabled = apEnabledEl ? apEnabledEl.checked : false;
  if (currentConfig && apEnabled !== currentConfig.ap_enabled) {
    partial.ap_enabled = apEnabled;
  }

  // AP SSID
  const apSsidEl = document.getElementById('netApSsid');
  if (apSsidEl && apSsidEl.value !== (currentConfig ? currentConfig.ap_ssid || '' : '')) {
    partial.ap_ssid = apSsidEl.value;
  }

  // AP password
  const apPwdEl = document.getElementById('netApPassword');
  if (apPwdEl && apPwdEl.value !== '') {
    partial.ap_password = apPwdEl.value;
  }

  // Access password
  const accessPwdEl = document.getElementById('netAccessPassword');
  const accessPwdConfirmEl = document.getElementById('netAccessPasswordConfirm');
  if (accessPwdEl && accessPwdEl.value !== '') {
    if (accessPwdConfirmEl && accessPwdEl.value !== accessPwdConfirmEl.value) {
      highlightField('netAccessPasswordConfirm', 'Les mots de passe ne correspondent pas');
      showFormMsg('netFormMsg', 'Les mots de passe ne correspondent pas', 'error');
      return;
    }
    partial.password = accessPwdEl.value;
  }

  // Contrainte C3 : si ap_enabled, alors ap_ssid non vide ET ap_password >= 8
  const effectiveApEnabled = partial.ap_enabled !== undefined ? partial.ap_enabled : (currentConfig ? currentConfig.ap_enabled : false);
  const effectiveApSsid = partial.ap_ssid !== undefined ? partial.ap_ssid : (currentConfig ? currentConfig.ap_ssid || '' : '');
  const effectiveApPwd = partial.ap_password !== undefined ? partial.ap_password : '';

  if (effectiveApEnabled) {
    if (!effectiveApSsid || effectiveApSsid.trim() === '') {
      highlightField('netApSsid', 'Le SSID du point d\'accès est requis lorsque le point d\'accès est activé');
      showFormMsg('netFormMsg', 'Le SSID du point d\'accès est requis', 'error');
      return;
    }
    const hasExistingPwd = currentConfig ? currentConfig.ap_password_set : false;
    if (effectiveApPwd !== '' && effectiveApPwd.length < 8) {
      highlightField('netApPassword', 'Le mot de passe du point d\'accès doit comporter au moins 8 caractères');
      showFormMsg('netFormMsg', 'Mot de passe AP : 8 caractères minimum', 'error');
      return;
    }
    if (effectiveApPwd === '' && !hasExistingPwd) {
      highlightField('netApPassword', 'Un mot de passe est requis pour le point d\'accès (8 caractères minimum)');
      showFormMsg('netFormMsg', 'Un mot de passe AP est requis (8 caractères minimum)', 'error');
      return;
    }
  }

  if (Object.keys(partial).length === 0) {
    showFormMsg('netFormMsg', 'Aucune modification à enregistrer', '');
    return;
  }

  setBtnLoading('netSaveBtn', true);
  showFormMsg('netFormMsg', 'Enregistrement en cours...', 'loading');

  try {
    const result = await saveConfig(partial);
    setBtnLoading('netSaveBtn', false, 'Enregistrer la configuration réseau');

    currentConfig = await getConfig();

    const pwdSetEl = document.getElementById('netWifiPasswordSet');
    const apPwdSetEl = document.getElementById('netApPasswordSet');
    if (pwdSetEl && currentConfig) pwdSetEl.textContent = currentConfig.wifi_password_set ? '(défini)' : '(non défini)';
    if (apPwdSetEl && currentConfig) apPwdSetEl.textContent = currentConfig.ap_password_set ? '(défini)' : '(non défini)';

    const pwdEl2 = document.getElementById('netWifiPassword');
    const apPwdEl2 = document.getElementById('netApPassword');
    if (pwdEl2) pwdEl2.value = '';
    if (apPwdEl2) apPwdEl2.value = '';

    const accessPwdSetEl2 = document.getElementById('netAccessPasswordSet');
    const accessPwdEl2 = document.getElementById('netAccessPassword');
    const accessPwdConfirmEl2 = document.getElementById('netAccessPasswordConfirm');
    if (accessPwdSetEl2 && currentConfig) accessPwdSetEl2.textContent = currentConfig.password_set ? '(défini)' : '(non défini)';
    if (accessPwdEl2) accessPwdEl2.value = '';
    if (accessPwdConfirmEl2) accessPwdConfirmEl2.value = '';

    if (result.reboot_required) {
      showFormMsg('netFormMsg', 'Configuration réseau enregistrée. Un redémarrage est nécessaire pour appliquer les changements.', 'success');
      proposeRestart('netFormMsg');
    } else {
      showFormMsg('netFormMsg', 'Configuration réseau enregistrée avec succès', 'success');
    }
  } catch (error) {
    setBtnLoading('netSaveBtn', false, 'Enregistrer la configuration réseau');

    if (error.code === 'VALIDATION_ERROR' && error.field) {
      const msg = highlightField(error.field, error.message, error.min, error.max);
      showFormMsg('netFormMsg', msg, 'error');
    } else {
      showFormMsg('netFormMsg', error.message || 'Erreur lors de l\'enregistrement', 'error');
    }
  }
}

// ---------------------------------------------------------------------------
// 4. Formulaire Intégrations
// ---------------------------------------------------------------------------

let integListenersBound = false;

/**
 * Charge et peuple le formulaire d'intégrations.
 * Appelé par app.js à l'ouverture de l'onglet Intégrations.
 */
export async function loadIntegrations() {
  clearFormMsg('integFormMsg');

  let cfg;
  try {
    cfg = await getConfig();
    currentConfig = cfg;
  } catch (e) {
    showFormMsg('integFormMsg', 'Erreur de chargement : ' + (e.message || 'Impossible de charger la configuration'), 'error');
    return;
  }

  if (!cfg) {
    showFormMsg('integFormMsg', 'Configuration non disponible', 'error');
    return;
  }

  // MQTT
  const mqttEnabledEl = document.getElementById('integMqttEnabled');
  const mqttBrokerEl = document.getElementById('integMqttBroker');
  const mqttPortEl = document.getElementById('integMqttPort');
  const mqttUserEl = document.getElementById('integMqttUser');
  const mqttPwdEl = document.getElementById('integMqttPassword');
  const mqttPwdSetEl = document.getElementById('integMqttPasswordSet');
  const mqttPrefixEl = document.getElementById('integMqttTopicPrefix');
  const mqttDeviceEl = document.getElementById('integMqttDeviceName');

  if (mqttEnabledEl) mqttEnabledEl.checked = cfg.mqtt_enabled === true;
  if (mqttBrokerEl) mqttBrokerEl.value = cfg.mqtt_broker || '';
  if (mqttPortEl) {
    mqttPortEl.value = cfg.mqtt_port != null ? cfg.mqtt_port : '';
    const mqttPortBound = cfg.bounds ? cfg.bounds.mqtt_port : null;
    if (mqttPortBound) {
      applyBounds('integMqttPort', mqttPortBound);
    } else {
      mqttPortEl.min = '1';
      mqttPortEl.max = '65535';
      mqttPortEl.step = '1';
    }
  }
  if (mqttUserEl) mqttUserEl.value = cfg.mqtt_user || '';
  if (mqttPwdEl) mqttPwdEl.value = '';
  if (mqttPwdSetEl) mqttPwdSetEl.textContent = cfg.mqtt_password_set ? '(défini)' : '(non défini)';
  if (mqttPrefixEl) mqttPrefixEl.value = cfg.mqtt_topic_prefix || '';
  if (mqttDeviceEl) mqttDeviceEl.value = cfg.mqtt_device_name || '';

  // Grainfather
  const gfEnabledEl = document.getElementById('integGfEnabled');
  const gfEndpointEl = document.getElementById('integGfEndpoint');
  const gfLabelEl = document.getElementById('integGfDeviceLabel');

  if (gfEnabledEl) gfEnabledEl.checked = cfg.gf_enabled === true;
  if (gfEndpointEl) gfEndpointEl.value = cfg.gf_endpoint || '';
  if (gfLabelEl) gfLabelEl.value = cfg.gf_device_label || '';

  // Réinitialiser les erreurs
  document.querySelectorAll('#panelIntegrations .field-error').forEach(function (el) {
    el.classList.remove('field-error');
  });

  // Écouteurs (une seule fois)
  if (!integListenersBound) {
    integListenersBound = true;
    setupIntegrationsListeners();
  }
}

function setupIntegrationsListeners() {
  const saveBtn = document.getElementById('integSaveBtn');

  if (saveBtn) {
    saveBtn.addEventListener('click', async function () {
      await handleIntegrationsSave();
    });
  }
}

async function handleIntegrationsSave() {
  clearFormMsg('integFormMsg');

  document.querySelectorAll('#panelIntegrations .field-error').forEach(function (el) {
    el.classList.remove('field-error');
  });

  const partial = {};

  // MQTT
  const mqttEnabledEl = document.getElementById('integMqttEnabled');
  const mqttBrokerEl = document.getElementById('integMqttBroker');
  const mqttPortEl = document.getElementById('integMqttPort');
  const mqttUserEl = document.getElementById('integMqttUser');
  const mqttPwdEl = document.getElementById('integMqttPassword');
  const mqttPrefixEl = document.getElementById('integMqttTopicPrefix');
  const mqttDeviceEl = document.getElementById('integMqttDeviceName');

  const mqttEnabled = mqttEnabledEl ? mqttEnabledEl.checked : false;

  if (currentConfig && mqttEnabled !== currentConfig.mqtt_enabled) {
    partial.mqtt_enabled = mqttEnabled;
  }
  if (mqttBrokerEl && mqttBrokerEl.value !== (currentConfig ? currentConfig.mqtt_broker || '' : '')) {
    partial.mqtt_broker = mqttBrokerEl.value;
  }
  if (mqttPortEl && mqttPortEl.value !== '' && Number(mqttPortEl.value) !== (currentConfig ? currentConfig.mqtt_port : null)) {
    const port = Number(mqttPortEl.value);
    if (isNaN(port) || port < 1 || port > 65535) {
      highlightField('integMqttPort', 'Port MQTT hors bornes (1–65535)', 1, 65535);
      showFormMsg('integFormMsg', 'Port MQTT hors bornes (1–65535)', 'error');
      return;
    }
    partial.mqtt_port = port;
  }
  if (mqttUserEl && mqttUserEl.value !== (currentConfig ? currentConfig.mqtt_user || '' : '')) {
    partial.mqtt_user = mqttUserEl.value;
  }
  if (mqttPwdEl && mqttPwdEl.value !== '') {
    partial.mqtt_password = mqttPwdEl.value;
  }
  if (mqttPrefixEl && mqttPrefixEl.value !== (currentConfig ? currentConfig.mqtt_topic_prefix || '' : '')) {
    partial.mqtt_topic_prefix = mqttPrefixEl.value;
  }
  if (mqttDeviceEl && mqttDeviceEl.value !== (currentConfig ? currentConfig.mqtt_device_name || '' : '')) {
    partial.mqtt_device_name = mqttDeviceEl.value;
  }

  // Grainfather
  const gfEnabledEl = document.getElementById('integGfEnabled');
  const gfEndpointEl = document.getElementById('integGfEndpoint');
  const gfLabelEl = document.getElementById('integGfDeviceLabel');

  const gfEnabled = gfEnabledEl ? gfEnabledEl.checked : false;

  if (currentConfig && gfEnabled !== currentConfig.gf_enabled) {
    partial.gf_enabled = gfEnabled;
  }
  if (gfEndpointEl && gfEndpointEl.value !== (currentConfig ? currentConfig.gf_endpoint || '' : '')) {
    partial.gf_endpoint = gfEndpointEl.value;
  }
  if (gfLabelEl && gfLabelEl.value !== (currentConfig ? currentConfig.gf_device_label || '' : '')) {
    partial.gf_device_label = gfLabelEl.value;
  }

  // Contrainte C4 : si gf_enabled, alors gf_endpoint commence par http:// ou https://
  const effectiveGfEnabled = partial.gf_enabled !== undefined ? partial.gf_enabled : (currentConfig ? currentConfig.gf_enabled : false);
  const effectiveGfEndpoint = partial.gf_endpoint !== undefined ? partial.gf_endpoint : (currentConfig ? currentConfig.gf_endpoint || '' : '');

  if (effectiveGfEnabled) {
    if (!effectiveGfEndpoint || (!effectiveGfEndpoint.startsWith('http://') && !effectiveGfEndpoint.startsWith('https://'))) {
      highlightField('integGfEndpoint', 'L\'endpoint Grainfather doit commencer par http:// ou https://');
      showFormMsg('integFormMsg', 'L\'endpoint Grainfather doit commencer par http:// ou https://', 'error');
      return;
    }
  }

  // Contrainte C5 : si mqtt_enabled, alors mqtt_broker non vide
  const effectiveMqttEnabled = partial.mqtt_enabled !== undefined ? partial.mqtt_enabled : (currentConfig ? currentConfig.mqtt_enabled : false);
  const effectiveMqttBroker = partial.mqtt_broker !== undefined ? partial.mqtt_broker : (currentConfig ? currentConfig.mqtt_broker || '' : '');

  if (effectiveMqttEnabled && (!effectiveMqttBroker || effectiveMqttBroker.trim() === '')) {
    highlightField('integMqttBroker', 'Le broker MQTT est requis lorsque MQTT est activé');
    showFormMsg('integFormMsg', 'Le broker MQTT est requis', 'error');
    return;
  }

  if (Object.keys(partial).length === 0) {
    showFormMsg('integFormMsg', 'Aucune modification à enregistrer', '');
    return;
  }

  setBtnLoading('integSaveBtn', true);
  showFormMsg('integFormMsg', 'Enregistrement en cours...', 'loading');

  try {
    const result = await saveConfig(partial);
    setBtnLoading('integSaveBtn', false, 'Enregistrer');

    currentConfig = await getConfig();

    const mqttPwdSetEl2 = document.getElementById('integMqttPasswordSet');
    if (mqttPwdSetEl2 && currentConfig) mqttPwdSetEl2.textContent = currentConfig.mqtt_password_set ? '(défini)' : '(non défini)';

    const mqttPwdEl2 = document.getElementById('integMqttPassword');
    if (mqttPwdEl2) mqttPwdEl2.value = '';

    if (result.reboot_required) {
      showFormMsg('integFormMsg', 'Configuration enregistrée. Un redémarrage est requis pour appliquer les changements.', 'success');
      proposeRestart('integFormMsg');
    } else {
      showFormMsg('integFormMsg', 'Configuration enregistrée avec succès', 'success');
    }
  } catch (error) {
    setBtnLoading('integSaveBtn', false, 'Enregistrer');

    if (error.code === 'VALIDATION_ERROR' && error.field) {
      const msg = highlightField(error.field, error.message, error.min, error.max);
      showFormMsg('integFormMsg', msg, 'error');
    } else {
      showFormMsg('integFormMsg', error.message || 'Erreur lors de l\'enregistrement', 'error');
    }
  }
}

// ---------------------------------------------------------------------------
// Helper : proposition de redémarrage
// ---------------------------------------------------------------------------

/**
 * Ajoute un lien/bouton de redémarrage après un message de formulaire.
 * @param {string} formMsgId - ID du conteneur de message
 */
function proposeRestart(formMsgId) {
  const el = document.getElementById(formMsgId);
  if (!el) return;

  if (el.querySelector('.restart-link')) return;

  const span = document.createElement('span');
  span.className = 'restart-link';
  span.style.display = 'block';
  span.style.marginTop = '8px';

  const btn = document.createElement('button');
  btn.type = 'button';
  btn.className = 'btn btn-danger';
  btn.textContent = 'Redémarrer maintenant';
  btn.addEventListener('click', async function () {
    if (!confirm('Redémarrer le contrôleur ? L\'interface sera momentanément indisponible.')) {
      return;
    }
    btn.disabled = true;
    btn.textContent = 'Redémarrage...';
    try {
      await restart();
      btn.textContent = 'Redémarrage en cours...';
    } catch (error) {
      btn.textContent = 'Échec — réessayer';
      btn.disabled = false;
    }
  });

  span.appendChild(btn);
  el.appendChild(span);
}
