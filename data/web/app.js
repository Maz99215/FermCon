// ===================================================================
// PROFIL DE TEMPÉRATURE
// ===================================================================

// Drapeau de saisie : armé quand l'utilisateur modifie le tableau
let profileDirty = false;
// État d'activation mémorisé depuis le serveur (bug 4)
let profileActiveState = false;

// Met à jour le libellé du bouton Enregistrer selon le drapeau
function updateSaveButtonLabel() {
    const btn = document.getElementById('saveProfileBtn');
    if (btn) {
        btn.textContent = profileDirty ? 'Enregistrer le profil *' : 'Enregistrer le profil';
    }
}

// Fonction pour charger le profil depuis le serveur
async function loadProfile() {
    try {
        const response = await fetch('/api/profile');
        const profile = await response.json();
        renderProfile(profile);
    } catch (error) {
        console.error('Erreur lors du chargement du profil:', error);
    }
}

// Fonction pour afficher le profil dans le tableau
function renderProfile(profile) {
    // Toujours mettre à jour les indicateurs temps réel
    document.getElementById('currentStep').textContent = profile.currentStep;
    document.getElementById('currentSetpoint').textContent = profile.setpoint;

    // Mémoriser l'état d'activation (bug 4)
    profileActiveState = profile.active;

    const activateBtn = document.getElementById('activateProfileBtn');
    if (activateBtn) {
        activateBtn.textContent = profileActiveState ? 'Désactiver le profil' : 'Activer le profil';
    }

    // Ne pas reconstruire le tableau si une saisie est en cours (bug 1)
    if (profileDirty) return;

    const tableBody = document.querySelector('#stepsTable tbody');
    tableBody.innerHTML = '';

    profile.steps.forEach(step => {
        const row = document.createElement('tr');
        // Cellules contenteditable (bug 3)
        row.innerHTML = `
            <td contenteditable="true">${step.type}</td>
            <td contenteditable="true">${step.tempStart}</td>
            <td contenteditable="true">${step.tempEnd}</td>
            <td contenteditable="true">${step.durationS / 60}</td>
        `;
        tableBody.appendChild(row);
    });
}

// Fonction pour enregistrer le profil
async function saveProfile() {
    const steps = [];
    const rows = document.querySelectorAll('#stepsTable tbody tr');

    rows.forEach(row => {
        const cells = row.querySelectorAll('td');
        steps.push({
            type: cells[0].textContent,
            tempStart: parseFloat(cells[1].textContent),
            tempEnd: parseFloat(cells[2].textContent),
            durationS: parseFloat(cells[3].textContent) * 60
        });
    });

    try {
        const response = await fetch('/api/profile', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ steps })
        });
        if (response.ok) {
            alert('Profil enregistré avec succès');
            // Désarmer le drapeau uniquement après succès, puis resynchroniser
            profileDirty = false;
            updateSaveButtonLabel();
            loadProfile();
        } else {
            alert('Erreur lors de l\'enregistrement du profil');
        }
    } catch (error) {
        console.error('Erreur lors de l\'enregistrement du profil:', error);
    }
}

// Fonction pour activer/désactiver le profil
async function toggleProfileActivation(active) {
    try {
        const response = await fetch('/api/profile/activate', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ active })
        });
        if (response.ok) {
            alert(`Profil ${active ? 'activé' : 'désactivé'} avec succès`);
            loadProfile(); // Rafraîchir les informations
        }
    } catch (error) {
        console.error('Erreur lors de la mise à jour du statut du profil:', error);
    }
}

// Événements
document.addEventListener('DOMContentLoaded', () => {
    // Charger le profil au chargement de la page
    loadProfile();

    // Rafraîchir le profil toutes les 5 secondes
    setInterval(loadProfile, 5000);

    // Bouton pour ajouter une étape — arme le drapeau
    document.getElementById('addStepBtn').addEventListener('click', () => {
        profileDirty = true;
        updateSaveButtonLabel();
        const tableBody = document.querySelector('#stepsTable tbody');
        const newRow = document.createElement('tr');
        newRow.innerHTML = `
            <td contenteditable="true">PALIER</td>
            <td contenteditable="true">0</td>
            <td contenteditable="true">0</td>
            <td contenteditable="true">0</td>
        `;
        tableBody.appendChild(newRow);
    });

    // Délégation d'événement : armer le drapeau sur toute saisie dans le tableau
    document.querySelector('#stepsTable tbody').addEventListener('input', () => {
        profileDirty = true;
        updateSaveButtonLabel();
    });

    // Bouton pour enregistrer le profil
    document.getElementById('saveProfileBtn').addEventListener('click', saveProfile);

    // Bouton pour activer/désactiver le profil — utilise l'état mémorisé (bug 4)
    document.getElementById('activateProfileBtn').addEventListener('click', () => {
        toggleProfileActivation(!profileActiveState);
    });
});

// ===================================================================
// FERMENTATION (libellé d'étape libre + jours de fermentation)
// ===================================================================
document.addEventListener('DOMContentLoaded', function() {
    // Drapeau de saisie pour le champ stageName
    var fermentationDirty = false;

    function refreshFermentationData() {
        fetch('/api/fermentation')
            .then(function(r) { return r.json(); })
            .then(function(data) {
                // Toujours mettre à jour le compteur de jours
                document.getElementById('fermentDays').textContent = 'Jour ' + data.fermentDays;

                // Ne réaffecter stageName que si pas de saisie en cours ET champ sans focus (bug 2)
                var stageField = document.getElementById('stageName');
                if (!fermentationDirty && document.activeElement !== stageField) {
                    stageField.value = data.stageName;
                }
            })
            .catch(function(err) { console.error('Erreur:', err); });
    }
    refreshFermentationData();
    setInterval(refreshFermentationData, 5000);

    function postFermentation(payload) {
        return fetch('/api/fermentation', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        }).then(function(r) { return r.json(); }).then(refreshFermentationData);
    }

    // Armer le drapeau quand l'utilisateur tape dans le champ
    document.getElementById('stageName').addEventListener('input', function() {
        fermentationDirty = true;
    });

    // Bouton Enregistrer : désarmer le drapeau uniquement après succès
    document.getElementById('saveButton').addEventListener('click', function() {
        postFermentation({ stageName: document.getElementById('stageName').value })
            .then(function() {
                fermentationDirty = false;
            });
    });

    // Boutons Démarrer / Réinitialiser : ne pas désarmer le drapeau
    document.getElementById('startButton').addEventListener('click', function() {
        postFermentation({ action: 'start' });
    });
    document.getElementById('resetButton').addEventListener('click', function() {
        postFermentation({ action: 'reset' });
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