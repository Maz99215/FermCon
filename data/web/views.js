/**
 * FermCon v0.4.0 — Tableau de bord (FE3)
 *
 * Export nommé : renderDashboard(status)
 *
 * Rafraîchit les indicateurs du tableau de bord à chaque cycle de polling.
 * Règles (CONTRACTS §6.4, §6.5, MODULES.md §FE3) :
 * - Valeurs NaN / null / absentes → afficher "--" sans exception
 * - Tout texte issu de l'API inséré par textContent, jamais innerHTML
 * - Seuls les nœuds dont la valeur a changé sont mis à jour (pas de scintillement)
 * - Les deux indicateurs FROID/CHAUD ne peuvent jamais apparaître actifs simultanément
 * - v0.4.0 : fusion des cartes Profil et Fermentation en une carte « Lot » unique
 */

// ---------------------------------------------------------------------------
// Cache des dernières valeurs rendues (prévention du scintillement)
// ---------------------------------------------------------------------------

/** @type {Record<string, string>} */
const lastValues = {};

/**
 * Met à jour le textContent d'un élément seulement si la valeur a changé.
 * @param {string} id - ID de l'élément DOM
 * @param {string} value - Nouvelle valeur textuelle
 * @param {string} [className] - Classe CSS optionnelle à appliquer
 */
function setIfChanged(id, value, className) {
  const el = document.getElementById(id);
  if (!el) return;

  const key = id;
  const strValue = String(value);

  if (lastValues[key] === strValue && (className === undefined || el.className === className)) {
    return;
  }

  el.textContent = strValue;
  lastValues[key] = strValue;

  if (className !== undefined) {
    el.className = className;
  }
}

/**
 * Formate une valeur numérique pour affichage.
 * Retourne "--" pour NaN, null, undefined.
 * @param {*} val - Valeur à formater
 * @param {number} [decimals=1] - Nombre de décimales
 * @param {string} [suffix=''] - Suffixe (unité)
 * @returns {string}
 */
function fmt(val, decimals, suffix) {
  if (decimals === undefined) decimals = 1;
  if (suffix === undefined) suffix = '';

  if (val === null || val === undefined || (typeof val === 'number' && isNaN(val))) {
    return '--';
  }

  if (typeof val === 'number') {
    return val.toFixed(decimals) + suffix;
  }

  return String(val);
}

/**
 * Formate l'uptime en jours, heures, minutes, secondes.
 * @param {number} seconds - Uptime en secondes
 * @returns {string}
 */
function fmtUptime(seconds) {
  if (seconds === null || seconds === undefined || typeof seconds !== 'number' || isNaN(seconds)) {
    return '--';
  }

  const d = Math.floor(seconds / 86400);
  const h = Math.floor((seconds % 86400) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = Math.floor(seconds % 60);

  const parts = [];
  if (d > 0) parts.push(d + 'j');
  if (h > 0 || d > 0) parts.push(h + 'h');
  parts.push(m + 'm');
  parts.push(s + 's');

  return parts.join(' ');
}

/**
 * Formate un timestamp epoch en date lisible, ou "--" si 0.
 * @param {number} epoch - Timestamp Unix en secondes
 * @returns {string}
 */
function fmtEpoch(epoch) {
  if (!epoch || typeof epoch !== 'number' || epoch < 1) {
    return '--';
  }
  try {
    const d = new Date(epoch * 1000);
    return d.toLocaleString('fr-FR', {
      day: '2-digit',
      month: '2-digit',
      year: 'numeric',
      hour: '2-digit',
      minute: '2-digit'
    });
  } catch (e) {
    return '--';
  }
}

/**
 * Formate le temps restant du lot avec l'unité la plus adaptée.
 * - >= 1 jour : jours + heures
 * - < 1 jour et >= 1 heure : heures + minutes
 * - < 1 heure : minutes seules
 * @param {number} remainingS - Temps restant en secondes, -1 = inconnu
 * @returns {string}
 */
function fmtRemaining(remainingS) {
  if (remainingS === null || remainingS === undefined || remainingS === -1 || typeof remainingS !== 'number' || isNaN(remainingS)) {
    return '--';
  }

  const d = Math.floor(remainingS / 86400);
  const h = Math.floor((remainingS % 86400) / 3600);
  const m = Math.floor((remainingS % 3600) / 60);

  // >= 1 jour : jours + heures
  if (d >= 1) {
    const parts = [d + ' j'];
    if (h > 0) parts.push(h + ' h');
    return parts.join(' ');
  }

  // < 1 jour et >= 1 heure : heures + minutes
  if (h >= 1) {
    const parts = [h + ' h'];
    if (m > 0) parts.push(m + ' min');
    return parts.join(' ');
  }

  // < 1 heure : minutes seules
  return m + ' min';
}


/**
 * Formate l'âge iSpindel en minutes.
 * @param {number} ageS - Âge en secondes, -1 = jamais vu
 * @returns {string}
 */
function fmtAge(ageS) {
  if (ageS === null || ageS === undefined || ageS === -1 || typeof ageS !== 'number' || isNaN(ageS)) {
    return '--';
  }
  const minutes = Math.floor(ageS / 60);
  return minutes + ' min';
}

// ---------------------------------------------------------------------------
// Rendu principal
// ---------------------------------------------------------------------------

/**
 * Rafraîchit le tableau de bord avec les données de /api/status.
 * Appelée par app.js à chaque cycle de polling réussi.
 *
 * @param {object} s - StatusResponse (CONTRACTS §2.1)
 */
export function renderDashboard(s) {
  if (!s || typeof s !== 'object') return;

  // --- Régulation ---
  setIfChanged('dashTempValue', fmt(s.temperature, 1, ' °C'));
  setIfChanged('dashSetpointValue', fmt(s.setpoint, 1, ' °C'));
  setIfChanged('dashStateValue', s.state || '--');

  // Indicateurs FROID / CHAUD — mutuellement exclusifs
  const fridgeOn = s.relay_fridge === true;
  const heaterOn = s.relay_heater === true;

  if (fridgeOn && heaterOn) {
    setIfChanged('dashRelayFridge', 'FROID', 'indicator indicator-cool');
    setIfChanged('dashRelayHeater', 'ARRÊT', 'indicator indicator-off');
  } else if (fridgeOn) {
    setIfChanged('dashRelayFridge', 'FROID', 'indicator indicator-cool');
    setIfChanged('dashRelayHeater', 'ARRÊT', 'indicator indicator-off');
  } else if (heaterOn) {
    setIfChanged('dashRelayFridge', 'ARRÊT', 'indicator indicator-off');
    setIfChanged('dashRelayHeater', 'CHAUD', 'indicator indicator-heat');
  } else {
    setIfChanged('dashRelayFridge', 'ARRÊT', 'indicator indicator-off');
    setIfChanged('dashRelayHeater', 'ARRÊT', 'indicator indicator-off');
  }

  // --- Sonde ---
  setIfChanged('dashSensorOk',
    s.temp_sensor_ok === true ? 'OK' : 'DÉFAUT',
    s.temp_sensor_ok === true ? 'data-value indicator-ok' : 'data-value indicator-error'
  );
  setIfChanged('dashFaultPending',
    s.fault_pending === true ? 'OUI' : 'Non',
    s.fault_pending === true ? 'data-value indicator-error' : 'data-value'
  );
  setIfChanged('dashFaultCount', String(s.fault_count ?? '--'));
  setIfChanged('dashHasValidReading',
    s.has_valid_reading === true ? 'Oui' : 'Non',
    s.has_valid_reading === true ? 'data-value indicator-ok' : 'data-value indicator-warn'
  );
  setIfChanged('dashLastFaultEpoch', fmtEpoch(s.last_fault_epoch));
  setIfChanged('dashLastRejectedReading', fmt(s.last_rejected_reading, 1, ' °C'));

  // --- iSpindel ---
  if (s.isp_age_s === -1 || s.isp_age_s === null || s.isp_age_s === undefined) {
    setIfChanged('dashIspOnline', 'Aucune donnée', 'data-value indicator-warn');
    setIfChanged('dashIspTemp', '--');
    setIfChanged('dashIspGravity', '--');
    setIfChanged('dashIspAngle', '--');
    setIfChanged('dashIspBattery', '--');
    setIfChanged('dashIspRssi', '--');
    setIfChanged('dashIspAge', '--');
  } else {
    setIfChanged('dashIspOnline',
      s.isp_online === true ? 'En ligne' : 'Hors ligne',
      s.isp_online === true ? 'data-value indicator-ok' : 'data-value indicator-warn'
    );
    setIfChanged('dashIspTemp', fmt(s.isp_temperature, 1, ' °C'));
    setIfChanged('dashIspGravity', fmt(s.isp_gravity, 3));
    setIfChanged('dashIspAngle', fmt(s.isp_angle, 1, '°'));
    setIfChanged('dashIspBattery', fmt(s.isp_battery, 2, ' V'));
    setIfChanged('dashIspRssi', s.isp_rssi && s.isp_rssi !== 0 ? s.isp_rssi + ' dBm' : '--');
    setIfChanged('dashIspAge', fmtAge(s.isp_age_s));
  }

  // --- Réseau ---
  setIfChanged('dashStaConnected',
    s.sta_connected === true ? 'Connecté' : 'Déconnecté',
    s.sta_connected === true ? 'data-value indicator-ok' : 'data-value indicator-warn'
  );
  setIfChanged('dashIpSta', s.ip_sta || '--');
  setIfChanged('dashIpAp', s.ip_ap || '--');
  setIfChanged('dashApClients', String(s.ap_clients ?? '--'));
  setIfChanged('dashWifiRssi', s.wifi_rssi != null ? s.wifi_rssi + ' dBm' : '--');
  setIfChanged('dashMqttConnected',
    s.mqtt_connected === true ? 'Connecté' : 'Déconnecté',
    s.mqtt_connected === true ? 'data-value indicator-ok' : 'data-value indicator-warn'
  );

  // --- Lot (fusion Profil + Fermentation, v0.4.0) ---
  // Nom du lot : utilise profile_step_label comme nom, ou "--"
  setIfChanged('dashBatchName', s.profile_step_label || '--');

  // Statut : actif ou inactif
  const batchActive = s.profile_active === true;
  setIfChanged('dashBatchStatus',
    batchActive ? 'Actif' : 'Inactif',
    batchActive ? 'data-value indicator-ok' : 'data-value'
  );

  // Jours écoulés
  setIfChanged('dashBatchDays', String(s.ferment_days ?? '--'));

  // Étape courante
  setIfChanged('dashBatchStage', s.stage_name || '--');

  // Progression : étape courante / total
  const stepIdx = (s.profile_step_index != null && s.profile_step_index !== undefined) ? s.profile_step_index : null;
  const stepCnt = (s.profile_step_count != null && s.profile_step_count !== undefined) ? s.profile_step_count : null;
  if (stepIdx !== null && stepCnt !== null && stepCnt > 0) {
    setIfChanged('dashBatchProgress', stepIdx + ' / ' + stepCnt);
  } else if (stepCnt === 0) {
    setIfChanged('dashBatchProgress', 'Sans étape');
  } else {
    setIfChanged('dashBatchProgress', '--');
  }

  // Temps restant
  setIfChanged('dashBatchRemaining', fmtRemaining(s.profile_remaining_s));

  // --- Système ---
  setIfChanged('dashFwVersion', s.fw_version || '--');
  setIfChanged('dashConfigVersion', 'v' + (s.config_version ?? '--'));
  setIfChanged('dashUptime', fmtUptime(s.uptime));
  setIfChanged('dashHeapFree', fmt(s.heap_free_kb, 0, ' kB'));
}
