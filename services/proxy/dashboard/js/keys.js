/* API key management UI */
var Dashboard = Dashboard || {};

Dashboard.keys = (function() {
    var grid = null;
    var emptyState = null;
    var editingKeyId = null;
    var initialized = false;

    function _bindEvents() {
        if (initialized) return;
        initialized = true;

        grid = document.getElementById('keys-grid');
        emptyState = document.getElementById('keys-empty');

        document.getElementById('btn-create-key').addEventListener('click', openCreateModal);
        document.getElementById('modal-key-close').addEventListener('click', closeKeyModal);
        document.getElementById('modal-key-cancel').addEventListener('click', closeKeyModal);
        document.getElementById('form-key').addEventListener('submit', handleSubmit);
        document.getElementById('modal-created-close').addEventListener('click', closeCreatedModal);
        document.getElementById('modal-created-done').addEventListener('click', closeCreatedModal);
        document.getElementById('btn-copy-key').addEventListener('click', copyKey);

        /* Slider value displays */
        bindSlider('key-temp', 'val-temp');
        bindSlider('key-topp', 'val-topp');
        bindSlider('key-freq', 'val-freq');
        bindSlider('key-pres', 'val-pres');
    }

    function mount() {
        _bindEvents();
        loadKeys();
    }

    function unmount() {
        /* no-op: no in-flight work to cancel */
    }

    function bindSlider(inputId, displayId) {
        var input = document.getElementById(inputId);
        var display = document.getElementById(displayId);
        input.addEventListener('input', function() {
            display.textContent = parseFloat(input.value).toFixed(1);
        });
    }

    function loadKeys() {
        Dashboard.api.get('/api/dashboard/keys').then(function(keys) {
            renderKeys(keys);
        }).catch(function(err) {
            console.error('Failed to load keys:', err);
        });
    }

    function renderKeys(keys) {
        grid.innerHTML = '';
        if (!keys || keys.length === 0) {
            emptyState.classList.remove('hidden');
            return;
        }
        emptyState.classList.add('hidden');

        keys.forEach(function(k) {
            var card = document.createElement('div');
            card.className = 'key-card' + (k.is_active ? '' : ' inactive');
            card.innerHTML =
                '<div class="key-card-header">' +
                    '<div>' +
                        '<div class="key-card-name">' + esc(k.name || 'Unnamed Key') + '</div>' +
                        '<div class="key-card-prefix">' + esc(k.key_prefix) + '</div>' +
                    '</div>' +
                    '<span class="key-card-model">' + esc(k.model) + '</span>' +
                '</div>' +
                '<div class="key-card-params">' +
                    '<div>Temp: <span>' + k.temperature.toFixed(1) + '</span></div>' +
                    '<div>Top P: <span>' + k.top_p.toFixed(2) + '</span></div>' +
                    '<div>Max Tokens: <span>' + k.max_tokens + '</span></div>' +
                    '<div>Reasoning: <span>' + esc(k.reasoning_effort) + '</span></div>' +
                '</div>' +
                '<div class="key-card-actions">' +
                    '<label class="toggle">' +
                        '<input type="checkbox"' + (k.is_active ? ' checked' : '') + '>' +
                        '<span class="toggle-slider"></span>' +
                    '</label>' +
                    '<button class="btn btn-sm btn-edit">Edit</button>' +
                    '<button class="btn btn-sm btn-danger btn-delete">Delete</button>' +
                '</div>';

            /* Toggle handler */
            card.querySelector('.toggle input').addEventListener('change', function() {
                Dashboard.api.post('/api/dashboard/keys/' + k.id + '/toggle', {})
                    .then(function() { loadKeys(); })
                    .catch(function(err) { alert(err.message); loadKeys(); });
            });

            /* Edit handler */
            card.querySelector('.btn-edit').addEventListener('click', function() {
                openEditModal(k);
            });

            /* Delete handler */
            card.querySelector('.btn-delete').addEventListener('click', function() {
                if (confirm('Delete this API key? This cannot be undone.')) {
                    Dashboard.api.del('/api/dashboard/keys/' + k.id)
                        .then(function() { loadKeys(); })
                        .catch(function(err) { alert(err.message); });
                }
            });

            grid.appendChild(card);
        });
    }

    function openCreateModal() {
        editingKeyId = null;
        document.getElementById('modal-key-title').textContent = 'Create API Key';
        document.getElementById('modal-key-submit').textContent = 'Create';
        resetForm();
        Dashboard.models.populateSelect(document.getElementById('key-model'), 'claude-sonnet-4.5');
        document.getElementById('modal-key').classList.remove('hidden');
    }

    function openEditModal(k) {
        editingKeyId = k.id;
        document.getElementById('modal-key-title').textContent = 'Edit API Key';
        document.getElementById('modal-key-submit').textContent = 'Save';
        document.getElementById('key-name').value = k.name || '';
        Dashboard.models.populateSelect(document.getElementById('key-model'), k.model);
        setSlider('key-temp', 'val-temp', k.temperature);
        setSlider('key-topp', 'val-topp', k.top_p);
        document.getElementById('key-maxtokens').value = k.max_tokens;
        setSlider('key-freq', 'val-freq', k.frequency_penalty);
        setSlider('key-pres', 'val-pres', k.presence_penalty);
        document.getElementById('key-reasoning').value = k.reasoning_effort;
        document.getElementById('key-sysprompt').value = k.system_prompt || '';
        document.getElementById('modal-key').classList.remove('hidden');
    }

    function setSlider(inputId, displayId, value) {
        var el = document.getElementById(inputId);
        el.value = value;
        document.getElementById(displayId).textContent = parseFloat(value).toFixed(1);
    }

    function resetForm() {
        document.getElementById('key-name').value = '';
        setSlider('key-temp', 'val-temp', 0.5);
        setSlider('key-topp', 'val-topp', 0.5);
        document.getElementById('key-maxtokens').value = 4096;
        setSlider('key-freq', 'val-freq', 0);
        setSlider('key-pres', 'val-pres', 0);
        document.getElementById('key-reasoning').value = 'medium';
        document.getElementById('key-sysprompt').value = '';
    }

    function closeKeyModal() {
        document.getElementById('modal-key').classList.add('hidden');
    }

    function closeCreatedModal() {
        document.getElementById('modal-created').classList.add('hidden');
    }

    function getFormData() {
        return {
            name: document.getElementById('key-name').value.trim(),
            model: document.getElementById('key-model').value,
            temperature: parseFloat(document.getElementById('key-temp').value),
            top_p: parseFloat(document.getElementById('key-topp').value),
            max_tokens: parseInt(document.getElementById('key-maxtokens').value, 10),
            frequency_penalty: parseFloat(document.getElementById('key-freq').value),
            presence_penalty: parseFloat(document.getElementById('key-pres').value),
            reasoning_effort: document.getElementById('key-reasoning').value,
            system_prompt: document.getElementById('key-sysprompt').value
        };
    }

    function handleSubmit(e) {
        e.preventDefault();
        var data = getFormData();

        if (editingKeyId) {
            Dashboard.api.put('/api/dashboard/keys/' + editingKeyId, data)
                .then(function() { closeKeyModal(); loadKeys(); })
                .catch(function(err) { alert(err.message); });
        } else {
            Dashboard.api.post('/api/dashboard/keys', data)
                .then(function(resp) {
                    closeKeyModal();
                    showCreatedKey(resp.key);
                    loadKeys();
                })
                .catch(function(err) { alert(err.message); });
        }
    }

    function showCreatedKey(key) {
        document.getElementById('created-key-value').textContent = key;
        document.getElementById('modal-created').classList.remove('hidden');
    }

    function copyKey() {
        var key = document.getElementById('created-key-value').textContent;
        navigator.clipboard.writeText(key).then(function() {
            document.getElementById('btn-copy-key').textContent = 'Copied';
            setTimeout(function() {
                document.getElementById('btn-copy-key').textContent = 'Copy';
            }, 2000);
        });
    }

    function esc(str) {
        var div = document.createElement('div');
        div.textContent = str;
        return div.innerHTML;
    }

    return {
        mount: mount,
        unmount: unmount,
        loadKeys: loadKeys
    };
})();
