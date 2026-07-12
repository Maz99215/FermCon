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
    const tableBody = document.querySelector('#stepsTable tbody');
    tableBody.innerHTML = '';

    profile.steps.forEach(step => {
        const row = document.createElement('tr');
        row.innerHTML = `
            <td>${step.type}</td>
            <td>${step.tempStart}</td>
            <td>${step.tempEnd}</td>
            <td>${step.durationS / 60}</td>
        `;
        tableBody.appendChild(row);
    });

    // Mettre à jour les informations de l'étape courante
    document.getElementById('currentStep').textContent = profile.currentStep;
    document.getElementById('currentSetpoint').textContent = profile.setpoint;
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

    // Bouton pour ajouter une étape
    document.getElementById('addStepBtn').addEventListener('click', () => {
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

    // Bouton pour enregistrer le profil
    document.getElementById('saveProfileBtn').addEventListener('click', saveProfile);

    // Bouton pour activer/désactiver le profil
    document.getElementById('activateProfileBtn').addEventListener('click', () => {
        const isActive = document.getElementById('activateProfileBtn').textContent === 'Activer le profil';
        toggleProfileActivation(isActive);
        document.getElementById('activateProfileBtn').textContent = isActive ? 'Désactiver le profil' : 'Activer le profil';
    });
});
// ===================================================================
// FERMENTATION (libellé d'étape libre + jours de fermentation)
// ===================================================================
document.addEventListener('DOMContentLoaded', function() {
    function refreshFermentationData() {
        fetch('/api/fermentation')
            .then(r => r.json())
            .then(data => {
                document.getElementById('stageName').value = data.stageName;
                document.getElementById('fermentDays').textContent = `Jour ${data.fermentDays}`;
            })
            .catch(err => console.error('Erreur:', err));
    }
    refreshFermentationData();
    setInterval(refreshFermentationData, 5000);

    function postFermentation(payload) {
        return fetch('/api/fermentation', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        }).then(r => r.json()).then(refreshFermentationData);
    }

    document.getElementById('saveButton').addEventListener('click', () =>
        postFermentation({ stageName: document.getElementById('stageName').value }));
    document.getElementById('startButton').addEventListener('click', () =>
        postFermentation({ action: 'start' }));
    document.getElementById('resetButton').addEventListener('click', () =>
        postFermentation({ action: 'reset' }));
});
