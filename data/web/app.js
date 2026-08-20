// ===================================================================
// PROFIL DE TEMPÉRATURE
// ===================================================================

let profileDirty = false;
let profileActiveState = false;

function updateSaveButtonLabel() {
    const btn = document.getElementById('saveProfileBtn');
    if (btn) { btn.textContent = profileDirty ? 'Enregistrer le profil *' : 'Enregistrer le profil'; }
}

function showFeedback(message, isError) {
    const fb = document.getElementById('profileFeedback');
    if (!fb) return;
    fb.textContent = message;
    fb.className = isError ? 'status-bar error' : 'status-bar connected';
    if (!isError) {
        setTimeout(function() {
            if (fb.textContent === message) { fb.textContent = ''; fb.className = ''; }
        }, 4000);
    }
}

function buildStepRow(step) {
    const row = document.createElement('tr');
    const typeVal = (step && step.type !== undefined) ? Number(step.type) : 0;
    const tempStartVal = (step && step.tempStart !== undefined) ? step.tempStart : 18;
    const tempEndVal = (step && step.tempEnd !== undefined) ? step.tempEnd : 18;
    const durationMin = (step && step.durationS !== undefined) ? (step.durationS / 60) : 60;
    const isPalier = (typeVal === 0);

    row.innerHTML =
        '<td><select class="step-type">' +
            '<option value="0"' + (typeVal === 0 ? ' selected' : '') + '>Palier</option>' +
            '<option value="1"' + (typeVal === 1 ? ' selected' : '') + '>Rampe</option>' +
        '</select></td>' +
        '<td><input type="number" class="step-temp-start" step="0.1" min="-10" max="40" value="' + tempStartVal + '"></td>' +
        '<td><input type="number" class="step-temp-end" step="0.1" min="-10" max="40" value="' + (isPalier ? tempStartVal : tempEndVal) + '"' + (isPalier ? ' disabled' : '') + '></td>' +
        '<td><input type="number" class="step-duration" step="1" min="1" max="100000" value="' + durationMin + '"></td>' +
        '<td><button type="button" class="step-delete">Supprimer</button></td>';

    return row;
}

function bindStepRowEvents(row) {
    const typeSelect = row.querySelector('.step-type');
    const tempStart = row.querySelector('.step-temp-start');
    const tempEnd = row.querySelector('.step-temp-end');

    typeSelect.addEventListener('change', function() {
        const isPalier = (parseInt(this.value, 10) === 0);
        tempEnd.disabled = isPalier;
        if (isPalier) {
            tempEnd.value = tempStart.value;
        }
        profileDirty = true;
        updateSaveButtonLabel();
    });
}

async function loadProfile() {
    try {
        const response = await fetch('/api/profile', { credentials: 'same-origin' });

        if (response.status === 401) {
            showFeedback('Authentification requise — rechargez la page et saisissez vos identifiants', true);
            return;
        }
        if (!response.ok) {
            showFeedback('HTTP ' + response.status, true);
            return;
        }

        const profile = await response.json();
        renderProfile(profile);
    } catch (error) {
        console.error('Erreur lors du chargement du profil:', error);
        showFeedback('Impossible de joindre le contrôleur', true);
    }
}

function renderProfile(profile) {
    document.getElementById('currentStep').textContent = profile.currentStep || '';
    document.getElementById('currentSetpoint').textContent = profile.setpoint !== undefined ? profile.setpoint : '--';

    profileActiveState = profile.active === true;
    const activateBtn = document.getElementById('activateProfileBtn');
    if (activateBtn) {
        activateBtn.textContent = profileActiveState ? 'Désactiver le profil' : 'Activer le profil';
    }

    // Ne jamais ecraser une saisie en cours (drapeau arme ou champ ayant le focus)
    if (profileDirty) return;

    const nameField = document.getElementById('profileName');
    if (nameField && document.activeElement !== nameField) {
        nameField.value = profile.name || '';
    }

    const tableBody = document.querySelector('#stepsTable tbody');
    tableBody.innerHTML = '';
    if (profile.steps && profile.steps.length > 0) {
        profile.steps.forEach(function(step) {
            const row = buildStepRow(step);
            bindStepRowEvents(row);
            tableBody.appendChild(row);
        });
    }
}

function validateSteps() {
    const rows = document.querySelectorAll('#stepsTable tbody tr');
    for (let i = 0; i < rows.length; i++) {
        const row = rows[i];
        const lineNum = i + 1;
        const typeVal = parseInt(row.querySelector('.step-type').value, 10);
        const tempStart = parseFloat(row.querySelector('.step-temp-start').value);
        const tempEnd = parseFloat(row.querySelector('.step-temp-end').value);
        const durationMin = parseFloat(row.querySelector('.step-duration').value);

        if (isNaN(tempStart) || !isFinite(tempStart)) {
            return 'Ligne ' + lineNum + ' : température début invalide.';
        }
        if (isNaN(tempEnd) || !isFinite(tempEnd)) {
            return 'Ligne ' + lineNum + ' : température fin invalide.';
        }
        if (isNaN(durationMin) || !isFinite(durationMin) || durationMin <= 0 || !Number.isInteger(durationMin)) {
            return 'Ligne ' + lineNum + ' : la durée doit être un entier de minutes strictement positif.';
        }
        if (typeVal === 1 && tempEnd === tempStart) {
            return 'Ligne ' + lineNum + ' : une rampe avec température de fin égale à la température de début n\'a pas de sens — utilisez un palier.';
        }
    }
    return null;
}

async function saveProfile() {
    const error = validateSteps();
    if (error) {
        showFeedback(error, true);
        return;
    }

    const rows = document.querySelectorAll('#stepsTable tbody tr');
    const steps = [];
    rows.forEach(function(row) {
        const typeVal = parseInt(row.querySelector('.step-type').value, 10);
        const tempStart = parseFloat(row.querySelector('.step-temp-start').value);
        let tempEnd = parseFloat(row.querySelector('.step-temp-end').value);
        const durationMin = parseFloat(row.querySelector('.step-duration').value);

        if (typeVal === 0) {
            tempEnd = tempStart;
        }

        steps.push({
            type: typeVal,
            tempStart: tempStart,
            tempEnd: tempEnd,
            durationS: Math.round(durationMin * 60)
        });
    });

    const payload = {
        name: document.getElementById('profileName').value,
        active: profileActiveState,
        steps: steps
    };

    try {
        const response = await fetch('/api/profile', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            credentials: 'same-origin',
            body: JSON.stringify(payload)
        });

        if (response.status === 401) {
            showFeedback('Authentification requise — rechargez la page et saisissez vos identifiants', true);
            return;
        }
        if (response.status === 400) {
            let msg = 'Requête invalide';
            try {
                const errData = await response.json();
                if (errData.error) msg = errData.error;
            } catch (e) { /* corps non JSON */ }
            showFeedback(msg, true);
            return;
        }
        if (!response.ok) {
            showFeedback('HTTP ' + response.status, true);
            return;
        }

        showFeedback('Profil enregistré avec succès', false);
        profileDirty = false;
        updateSaveButtonLabel();
        loadProfile();
    } catch (error) {
        console.error('Erreur lors de l\'enregistrement du profil:', error);
        showFeedback('Impossible de joindre le contrôleur', true);
    }
}

async function toggleProfileActivation(active) {
    if (profileDirty) {
        if (!confirm('Des modifications non enregistrées seront perdues. Continuer ?')) {
            return;
        }
    }

    const btn = document.getElementById('activateProfileBtn');
    if (btn) btn.disabled = true;

    try {
        const response = await fetch('/api/profile/activate', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            credentials: 'same-origin',
            body: JSON.stringify({ active: active })
        });

        if (response.status === 401) {
            showFeedback('Authentification requise — rechargez la page et saisissez vos identifiants', true);
            return;
        }
        if (response.status === 400) {
            let msg = 'Requête invalide';
            try {
                const errData = await response.json();
                if (errData.error) msg = errData.error;
            } catch (e) { /* corps non JSON */ }
            showFeedback(msg, true);
            return;
        }
        if (!response.ok) {
            showFeedback('HTTP ' + response.status, true);
            return;
        }

        showFeedback('Profil ' + (active ? 'activé' : 'désactivé') + ' avec succès', false);
        profileDirty = false;
        updateSaveButtonLabel();
        loadProfile();
    } catch (error) {
        console.error('Erreur lors de la mise à jour du statut du profil:', error);
        showFeedback('Impossible de joindre le contrôleur', true);
    } finally {
        if (btn) btn.disabled = false;
    }
}

document.addEventListener('DOMContentLoaded', function() {
    loadProfile();
    setInterval(loadProfile, 5000);

    document.getElementById('addStepBtn').addEventListener('click', function() {
        const tableBody = document.querySelector('#stepsTable tbody');
        if (tableBody.querySelectorAll('tr').length >= 16) {
            showFeedback('Maximum 16 étapes autorisées.', true);
            return;
        }
        profileDirty = true;
        updateSaveButtonLabel();
        const row = buildStepRow({ type: 0, tempStart: 18, tempEnd: 18, durationS: 3600 });
        bindStepRowEvents(row);
        tableBody.appendChild(row);
    });

    document.querySelector('#stepsTable tbody').addEventListener('click', function(e) {
        if (e.target && e.target.classList.contains('step-delete')) {
            const row = e.target.closest('tr');
            if (row) {
                row.remove();
                profileDirty = true;
                updateSaveButtonLabel();
            }
        }
    });

    document.querySelector('#stepsTable tbody').addEventListener('input', function() {
        profileDirty = true;
        updateSaveButtonLabel();
    });

    document.getElementById('saveProfileBtn').addEventListener('click', saveProfile);

    document.getElementById('activateProfileBtn').addEventListener('click', function() {
        toggleProfileActivation(!profileActiveState);
    });
});

// ===================================================================
// FERMENTATION (libellé d'étape libre + jours de fermentation)
// ===================================================================
document.addEventListener('DOMContentLoaded', function() {
    var fermentDirty = false;
    var fermentStarted = false;

    function formatDate(epoch) {
        var d = new Date(epoch * 1000);
        var day     = ('0' + d.getDate()).slice(-2);
        var month   = ('0' + (d.getMonth() + 1)).slice(-2);
        var year    = d.getFullYear();
        var hours   = ('0' + d.getHours()).slice(-2);
        var minutes = ('0' + d.getMinutes()).slice(-2);
        return day + '/' + month + '/' + year + ' à ' + hours + ':' + minutes;
    }

    function updateFermentDisplay(data) {
        var el = document.getElementById('fermentDays');
        if (!data.started) {
            el.textContent = 'Lot non démarré';
        } else if (data.startEpoch > 1600000000) {
            el.textContent = 'Jour ' + data.fermentDays + ' — démarré le ' + formatDate(data.startEpoch);
        } else {
            el.textContent = 'Jour ' + data.fermentDays + ' (date de début inconnue)';
        }
    }

    function updateButtons(data) {
        var startBtn = document.getElementById('startButton');
        var resetBtn = document.getElementById('resetButton');
        fermentStarted = data.started;
        if (data.started) {
            startBtn.textContent = 'Redémarrer le lot';
            resetBtn.disabled = false;
        } else {
            startBtn.textContent = 'Démarrer le lot';
            resetBtn.disabled = true;
        }
    }

    function applyFermentationData(data) {
        updateFermentDisplay(data);
        updateButtons(data);
        var stageField = document.getElementById('stageName');
        if (!fermentDirty && document.activeElement !== stageField) {
            stageField.value = data.stageName;
        }
    }

    function refreshFermentationData() {
        var httpHandled = false;
        fetch('/api/fermentation', { credentials: 'same-origin' })
            .then(function(r) {
                if (r.status === 401) {
                    document.getElementById('fermentDays').textContent = 'Authentification requise';
                    httpHandled = true;
                    throw new Error('HTTP 401');
                }
                if (!r.ok) {
                    document.getElementById('fermentDays').textContent = 'Erreur HTTP ' + r.status;
                    httpHandled = true;
                    throw new Error('HTTP ' + r.status);
                }
                return r.json();
            })
            .then(applyFermentationData)
            .catch(function(err) {
                if (!httpHandled) {
                    document.getElementById('fermentDays').textContent = 'Contrôleur injoignable';
                }
                console.error('Erreur:', err);
            });
    }
    refreshFermentationData();
    setInterval(refreshFermentationData, 5000);

    function postFermentation(payload) {
        var httpHandled = false;
        return fetch('/api/fermentation', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            credentials: 'same-origin',
            body: JSON.stringify(payload)
        })
        .then(function(r) {
            if (r.status === 401) {
                document.getElementById('fermentDays').textContent = 'Authentification requise';
                httpHandled = true;
                throw new Error('HTTP 401');
            }
            if (!r.ok) {
                document.getElementById('fermentDays').textContent = 'Erreur HTTP ' + r.status;
                httpHandled = true;
                throw new Error('HTTP ' + r.status);
            }
            return r.json();
        })
        .then(applyFermentationData)
        .catch(function(err) {
            if (!httpHandled) {
                document.getElementById('fermentDays').textContent = 'Contrôleur injoignable';
            }
            console.error('Erreur:', err);
            throw err;
        });
    }

    document.getElementById('stageName').addEventListener('input', function() {
        fermentDirty = true;
    });

    document.getElementById('saveButton').addEventListener('click', function() {
        postFermentation({ stageName: document.getElementById('stageName').value })
            .then(function() { fermentDirty = false; })
            .catch(function() { /* erreur deja traitee dans postFermentation */ });
    });

    document.getElementById('startButton').addEventListener('click', function() {
        if (fermentStarted) {
            if (!confirm('Un lot est déjà en cours. La date de début actuelle sera remplacée et le compteur de jours repartira de zéro. Continuer ?')) {
                return;
            }
        }
        postFermentation({ action: 'start' }).catch(function() {});
    });

    document.getElementById('resetButton').addEventListener('click', function() {
        if (!confirm('La date de début sera effacée. Continuer ?')) {
            return;
        }
        postFermentation({ action: 'reset' }).catch(function() {});
    });
});

// ===================================================================
// RESEAU (etat STA/AP + configuration Wi-Fi)
// ===================================================================
document.addEventListener('DOMContentLoaded', function() {

    // --- Etat reseau (lecture seule, rafraichi automatiquement) ---

    function refreshNetworkStatus() {
        fetch('/api/status', { credentials: 'same-origin' })
            .then(function(r) {
                if (r.status === 401) {
                    var bar = document.getElementById('netStatusBar');
                    bar.textContent = 'Authentification requise — rechargez la page et saisissez vos identifiants';
                    bar.className = 'status-bar error';
                    return null;
                }
                if (!r.ok) throw new Error('HTTP ' + r.status);
                return r.json();
            })
            .then(function(data) {
                if (!data) return;

                var bar = document.getElementById('netStatusBar');
                if (data.sta_connected) {
                    bar.textContent = 'Connecté à la box';
                    bar.className = 'status-bar connected';
                } else {
                    bar.textContent = 'Hors ligne — point d\'accès seul';
                    bar.className = 'status-bar error';
                }

                document.getElementById('netIpSta').textContent =
                    data.sta_connected ? data.ip_sta : '—';
                document.getElementById('netIpAp').textContent =
                    data.ip_ap || '—';
                document.getElementById('netApClients').textContent =
                    (data.ap_clients != null ? data.ap_clients : '—');
                document.getElementById('netRssi').textContent =
                    (data.wifi_rssi && data.wifi_rssi !== 0) ? data.wifi_rssi + ' dBm' : '—';
            })
            .catch(function(err) {
                console.error('Erreur refreshNetworkStatus:', err);
                var bar = document.getElementById('netStatusBar');
                bar.textContent = 'Impossible de joindre le contrôleur';
                bar.className = 'status-bar error';
            });
    }

    refreshNetworkStatus();
    var networkRefreshInterval = setInterval(refreshNetworkStatus, 5000);

    // --- Chargement de la configuration (une seule fois) ---

    function loadNetworkConfig() {
        fetch('/api/config', { credentials: 'same-origin' })
            .then(function(r) {
                if (r.status === 401) {
                    var fb401 = document.getElementById('netFeedback');
                    fb401.textContent = 'Authentification requise — rechargez la page et saisissez vos identifiants';
                    fb401.className = 'status-bar error';
                    return null;
                }
                if (!r.ok) throw new Error('HTTP ' + r.status);
                return r.json();
            })
            .then(function(data) {
                if (!data) return;

                document.getElementById('cfgWifiSsid').value = data.wifi_ssid || '';
                document.getElementById('cfgApSsid').value = data.ap_ssid || '';
                document.getElementById('cfgApEnabled').checked = !!data.ap_enabled;

                if (data.ap_password_set) {
                    document.getElementById('cfgApPassword').placeholder =
                        'mot de passe défini — laisser vide pour le conserver';
                }
            })
            .catch(function(err) {
                console.error('Erreur loadNetworkConfig:', err);
                var fb = document.getElementById('netFeedback');
                fb.textContent = 'Impossible de charger la configuration';
                fb.className = 'status-bar error';
            });
    }

    loadNetworkConfig();

    // --- Enregistrement de la configuration reseau ---

    document.getElementById('saveNetworkBtn').addEventListener('click', function() {
        var fb = document.getElementById('netFeedback');
        fb.textContent = '';
        fb.className = '';

        var apEnabled = document.getElementById('cfgApEnabled').checked;
        var apSsid = document.getElementById('cfgApSsid').value.trim();
        // Les mots de passe ne sont jamais rognes : un espace peut en faire partie
        var apPassword = document.getElementById('cfgApPassword').value;
        var wifiPassword = document.getElementById('cfgWifiPassword').value;

        // Validation cote client
        if (apEnabled && !apSsid) {
            fb.textContent = 'Le SSID du point d\'accès ne peut pas être vide lorsque l\'AP est activé.';
            fb.className = 'status-bar error';
            return;
        }
        if (apPassword.length > 0 && apPassword.length < 8) {
            fb.textContent = 'Le mot de passe du point d\'accès doit contenir au moins 8 caractères.';
            fb.className = 'status-bar error';
            return;
        }

        var payload = {
            wifi_ssid: document.getElementById('cfgWifiSsid').value.trim(),
            ap_enabled: apEnabled,
            ap_ssid: apSsid
        };

        if (wifiPassword.length > 0) {
            payload.wifi_password = wifiPassword;
        }
        if (apPassword.length > 0) {
            payload.ap_password = apPassword;
        }

        fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            credentials: 'same-origin',
            body: JSON.stringify(payload)
        })
        .then(function(r) {
            if (r.status === 401) {
                fb.textContent = 'Authentification requise — rechargez la page et saisissez vos identifiants';
                fb.className = 'status-bar error';
                return null;
            }
            if (r.status === 400) {
                return r.json().then(function(errData) {
                    throw new Error(errData.error || 'Erreur de validation');
                });
            }
            if (!r.ok) throw new Error('HTTP ' + r.status);
            return r.json();
        })
        .then(function(data) {
            if (!data) return;

            var msg = 'Configuration enregistrée.';
            if (data.reboot_required) {
                msg += ' Les paramètres du point d\'accès ne seront appliqués qu\'après redémarrage du contrôleur.';
            }
            fb.textContent = msg;
            fb.className = 'status-bar connected';
            if (data.reboot_required) {
                restartBtn.textContent = '⚠ Redémarrer le contrôleur (requis)';
            }

            document.getElementById('cfgWifiPassword').value = '';
            document.getElementById('cfgApPassword').value = '';

            loadNetworkConfig();
        })
        .catch(function(err) {
            console.error('Erreur saveNetworkConfig:', err);
            fb.textContent = err.message || 'Erreur réseau — impossible de joindre le contrôleur';
            fb.className = 'status-bar error';
        });
    });
// --- Redemarrage du controleur ---
    var restartBtn = document.getElementById('restartBtn');
    var restartFeedback = document.getElementById('restartFeedback');
    var countdownTimer = null;

    restartBtn.addEventListener('click', function() {
        var ok = confirm(
            'Redémarrer le contrôleur ?\n\n' +
            'ATTENTION : la régulation de température est interrompue pendant ' +
            'environ 5 secondes et le relais retombe. Le frigo et la plaque ' +
            'chauffante seront coupés temporairement.'
        );
        if (!ok) return;

        restartBtn.disabled = true;
        restartFeedback.textContent = 'Envoi de la commande…';
        restartFeedback.className = 'status-bar';

        fetch('/api/restart', { method: 'POST', credentials: 'same-origin' })
            .then(function(resp) {
                if (resp.status === 401) {
                    restartBtn.disabled = false;
                    restartFeedback.textContent = 'Authentification requise — rechargez la page et saisissez vos identifiants';
                    restartFeedback.className = 'status-bar error';
                    return null;
                }
                if (!resp.ok) throw new Error('HTTP ' + resp.status);
                return resp.json();
            })
            .then(function(data) {
                if (!data) return;

                // Suspendre le rafraichissement : il echouerait pendant le reboot
                clearInterval(networkRefreshInterval);

                var remaining = 10;
                restartFeedback.textContent = 'Redémarrage en cours… retour en ligne dans ' + remaining + ' s';
                restartFeedback.className = 'status-bar connected';

                countdownTimer = setInterval(function() {
                    remaining--;
                    if (remaining <= 0) {
                        clearInterval(countdownTimer);
                        location.reload();
                        return;
                    }
                    restartFeedback.textContent = 'Redémarrage en cours… retour en ligne dans ' + remaining + ' s';
                }, 1000);
            })
            .catch(function(err) {
                console.error('Erreur redemarrage:', err);
                restartBtn.disabled = false;
                restartFeedback.textContent = 'Échec de la commande de redémarrage.';
                restartFeedback.className = 'status-bar error';
                if (countdownTimer) {
                    clearInterval(countdownTimer);
                    countdownTimer = null;
                }
            });
    });
});