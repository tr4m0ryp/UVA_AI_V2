/* API key management UI -- table layout with detail/usage view */
var Dashboard = Dashboard || {};

Dashboard.keys = (function() {
    var tbody = null;
    var tableWrap = null;
    var emptyState = null;
    var listView = null;
    var detailView = null;
    var editingKeyId = null;
    var cachedKeys = [];
    var initialized = false;

    function _bindEvents() {
        if (initialized) return;
        initialized = true;

        tbody = document.getElementById('keys-tbody');
        tableWrap = document.getElementById('keys-table-wrap');
        emptyState = document.getElementById('keys-empty');
        listView = document.getElementById('keys-list-view');
        detailView = document.getElementById('keys-detail-view');

        document.getElementById('btn-create-key').addEventListener('click', openCreateModal);
        document.getElementById('modal-key-close').addEventListener('click', closeKeyModal);
        document.getElementById('modal-key-cancel').addEventListener('click', closeKeyModal);
        document.getElementById('form-key').addEventListener('submit', handleSubmit);
        document.getElementById('modal-created-close').addEventListener('click', closeCreatedModal);
        document.getElementById('modal-created-done').addEventListener('click', closeCreatedModal);
        document.getElementById('btn-copy-key').addEventListener('click', copyKey);
        document.getElementById('key-detail-back').addEventListener('click', showList);

        bindSlider('key-temp', 'val-temp');
        bindSlider('key-topp', 'val-topp');
        bindSlider('key-freq', 'val-freq');
        bindSlider('key-pres', 'val-pres');

        /* Guide toggle and tabs */
        document.getElementById('guide-toggle').addEventListener('click', toggleGuide);
        var tabs = document.querySelectorAll('.guide-tab');
        for (var i = 0; i < tabs.length; i++) {
            tabs[i].addEventListener('click', switchGuideTab);
        }
        var copyBtns = document.querySelectorAll('.guide-copy-btn');
        for (var j = 0; j < copyBtns.length; j++) {
            copyBtns[j].addEventListener('click', handleCopySnippet);
        }
    }

    function mount() {
        _bindEvents();
        showList();
        loadKeys();
    }

    function unmount() {}

    function bindSlider(inputId, displayId) {
        var input = document.getElementById(inputId);
        var display = document.getElementById(displayId);
        input.addEventListener('input', function() {
            display.textContent = parseFloat(input.value).toFixed(1);
        });
    }

    function loadKeys() {
        Dashboard.api.get('/api/dashboard/keys').then(function(keys) {
            cachedKeys = keys || [];
            renderKeys(cachedKeys);
        }).catch(function(err) {
            console.error('Failed to load keys:', err);
        });
    }

    /* --- List View --- */

    function showList() {
        listView.classList.remove('hidden');
        detailView.classList.add('hidden');
    }

    function formatDate(dateStr) {
        if (!dateStr) return '--';
        var d = new Date(dateStr + 'Z');
        if (isNaN(d.getTime())) return '--';
        var m = d.toLocaleString('en-US', { month: 'short' });
        return m + ' ' + d.getDate() + ', ' + d.getFullYear();
    }

    function renderKeys(keys) {
        tbody.innerHTML = '';
        var guideEl = document.getElementById('keys-guide');
        if (!keys || keys.length === 0) {
            tableWrap.classList.add('hidden');
            emptyState.classList.remove('hidden');
            guideEl.classList.add('hidden');
            return;
        }
        emptyState.classList.add('hidden');
        tableWrap.classList.remove('hidden');
        guideEl.classList.remove('hidden');
        renderGuide();

        keys.forEach(function(k) {
            var tr = document.createElement('tr');
            if (!k.is_active) tr.className = 'inactive';

            var statusClass = k.is_active ? 'active' : 'disabled';
            var statusText = k.is_active ? 'Active' : 'Inactive';

            tr.innerHTML =
                '<td><div class="key-name-cell">' +
                    '<span class="key-name-text">' + esc(k.name || 'Unnamed Key') + '</span>' +
                    '<span class="key-prefix-text">' + esc(k.key_prefix) + '</span>' +
                '</div></td>' +
                '<td><span class="badge-status ' + statusClass + '">' +
                    '<span class="dot"></span>' + statusText + '</span></td>' +
                '<td class="hide-mobile"><span class="key-date">' +
                    formatDate(k.created_at) + '</span></td>' +
                '<td class="hide-mobile"><span class="key-date">' +
                    formatDate(k.last_used) + '</span></td>' +
                '<td><div class="key-actions-cell">' +
                    '<button class="btn-icon btn-toggle" title="Toggle">' +
                        '<span class="material-symbols-outlined">' +
                        (k.is_active ? 'toggle_on' : 'toggle_off') + '</span></button>' +
                    '<button class="btn-icon btn-edit" title="Edit">' +
                        '<span class="material-symbols-outlined">edit</span></button>' +
                    '<button class="btn-icon danger btn-delete" title="Delete">' +
                        '<span class="material-symbols-outlined">delete</span></button>' +
                '</div></td>';

            /* Click row to open detail (but not action buttons) */
            tr.addEventListener('click', function(e) {
                if (e.target.closest('.key-actions-cell')) return;
                openDetail(k);
            });

            /* Toggle */
            tr.querySelector('.btn-toggle').addEventListener('click', function(e) {
                e.stopPropagation();
                Dashboard.api.post('/api/dashboard/keys/' + k.id + '/toggle', {})
                    .then(function() { loadKeys(); })
                    .catch(function(err) { alert(err.message); loadKeys(); });
            });

            /* Edit */
            tr.querySelector('.btn-edit').addEventListener('click', function(e) {
                e.stopPropagation();
                openEditModal(k);
            });

            /* Delete */
            tr.querySelector('.btn-delete').addEventListener('click', function(e) {
                e.stopPropagation();
                if (confirm('Delete this API key? This cannot be undone.')) {
                    Dashboard.api.del('/api/dashboard/keys/' + k.id)
                        .then(function() { loadKeys(); })
                        .catch(function(err) { alert(err.message); });
                }
            });

            tbody.appendChild(tr);
        });
    }

    /* --- Guide --- */

    function getBaseUrl() {
        return window.location.protocol + '//' + window.location.host;
    }

    function renderGuide() {
        var base = getBaseUrl();
        var el = document.getElementById('guide-base-url');
        if (el) el.textContent = base;

        /* Populate model selector */
        var select = document.getElementById('guide-model-select');
        if (select) {
            Dashboard.models.populateSelect(select, 'gpt-4o');
            select.removeEventListener('change', updateGuideSnippets);
            select.addEventListener('change', updateGuideSnippets);
        }

        updateGuideSnippets();
    }

    function updateGuideSnippets() {
        var base = getBaseUrl();
        var select = document.getElementById('guide-model-select');
        var model = (select && select.value) ? select.value : 'gpt-4o';

        setText('guide-codex-config',
            'model_provider = "uva_proxy"\n' +
            'model = "' + model + '"\n\n' +
            '[model_providers.uva_proxy]\n' +
            'name = "UvA Proxy"\n' +
            'base_url = "' + base + '/v1"\n' +
            'env_key = "UVA_PROXY_API_KEY"\n' +
            'wire_api = "responses"');

        setText('guide-codex-env',
            'export UVA_PROXY_API_KEY="uva-sk-your-key-here"\n' +
            'codex "Explain this codebase"');

        setText('guide-continue-config',
            'models:\n' +
            '  - name: ' + model + '\n' +
            '    provider: openai\n' +
            '    apiBase: ' + base + '/v1/\n' +
            '    apiKey: uva-sk-your-key-here\n' +
            '    model: ' + model);

        setText('guide-vscode-config',
            '// settings.json\n' +
            '{\n' +
            '  "github.copilot.chat.customOAIModels": {\n' +
            '    "' + model + '": {\n' +
            '      "name": "UvA ' + model + '",\n' +
            '      "url": "' + base + '/v1/chat/completions",\n' +
            '      "requiresAPIKey": true,\n' +
            '      "toolCalling": true,\n' +
            '      "vision": true,\n' +
            '      "maxInputTokens": 128000,\n' +
            '      "maxOutputTokens": 16384\n' +
            '    }\n' +
            '  }\n' +
            '}');

        setText('guide-curl-chat',
            'curl ' + base + '/v1/chat/completions \\\n' +
            '  -H "Authorization: Bearer uva-sk-your-key-here" \\\n' +
            '  -H "Content-Type: application/json" \\\n' +
            '  -d \'{\n' +
            '    "model": "' + model + '",\n' +
            '    "stream": true,\n' +
            '    "messages": [\n' +
            '      {"role": "user", "content": "Hello!"}\n' +
            '    ]\n' +
            '  }\'');

        setText('guide-curl-models',
            'curl ' + base + '/v1/models');

        setText('guide-python-code',
            'from openai import OpenAI\n\n' +
            'client = OpenAI(\n' +
            '    base_url="' + base + '/v1",\n' +
            '    api_key="uva-sk-your-key-here",\n' +
            ')\n\n' +
            'response = client.chat.completions.create(\n' +
            '    model="' + model + '",\n' +
            '    messages=[{"role": "user", "content": "Hello!"}],\n' +
            ')\n' +
            'print(response.choices[0].message.content)');
    }

    function setText(id, text) {
        var el = document.getElementById(id);
        if (el) el.textContent = text;
    }

    function toggleGuide() {
        var content = document.getElementById('guide-content');
        var chevron = document.querySelector('.guide-chevron');
        content.classList.toggle('hidden');
        if (chevron) {
            chevron.textContent = content.classList.contains('hidden')
                ? 'expand_more' : 'expand_less';
        }
    }

    function switchGuideTab(e) {
        var tab = e.target.getAttribute('data-guide-tab');
        if (!tab) return;

        var tabs = document.querySelectorAll('.guide-tab');
        for (var i = 0; i < tabs.length; i++)
            tabs[i].classList.remove('active');
        e.target.classList.add('active');

        var panels = document.querySelectorAll('.guide-panel');
        for (var j = 0; j < panels.length; j++) {
            if (panels[j].getAttribute('data-guide-panel') === tab)
                panels[j].classList.remove('hidden');
            else
                panels[j].classList.add('hidden');
        }
    }

    function handleCopySnippet(e) {
        var targetId = e.target.getAttribute('data-copy-target');
        if (!targetId) return;
        var el = document.getElementById(targetId);
        if (!el) return;
        var text = el.textContent;
        navigator.clipboard.writeText(text).then(function() {
            var btn = e.target;
            btn.textContent = 'Copied';
            setTimeout(function() { btn.textContent = 'Copy'; }, 2000);
        });
    }

    /* --- Detail View --- */

    function openDetail(key) {
        listView.classList.add('hidden');
        detailView.classList.remove('hidden');

        /* Render header */
        var header = document.getElementById('key-detail-header');
        header.innerHTML =
            '<div class="key-detail-title-row">' +
                '<span class="key-detail-name">' + esc(key.name || 'Unnamed Key') + '</span>' +
            '</div>' +
            '<div class="key-detail-meta">' +
                '<div class="key-detail-meta-item">Prefix: <strong>' +
                    esc(key.key_prefix) + '</strong></div>' +
                '<div class="key-detail-meta-item">Status: <strong>' +
                    (key.is_active ? 'Active' : 'Inactive') + '</strong></div>' +
                '<div class="key-detail-meta-item">Created: <strong>' +
                    formatDate(key.created_at) + '</strong></div>' +
                '<div class="key-detail-meta-item">Last Used: <strong>' +
                    formatDate(key.last_used) + '</strong></div>' +
            '</div>' +
            '<div class="key-detail-params">' +
                paramItem('Temperature', key.temperature.toFixed(1)) +
                paramItem('Top P', key.top_p.toFixed(2)) +
                paramItem('Max Tokens', key.max_tokens) +
                paramItem('Freq Penalty', key.frequency_penalty.toFixed(1)) +
                paramItem('Pres Penalty', key.presence_penalty.toFixed(1)) +
                paramItem('Reasoning', key.reasoning_effort) +
            '</div>';

        /* Fetch usage stats */
        loadUsage(key.id);
    }

    function paramItem(label, value) {
        return '<div class="key-param-item">' + esc(label) +
               '<span>' + esc(String(value)) + '</span></div>';
    }

    function fmtNumber(n) {
        if (n >= 1000000) return (n / 1000000).toFixed(1) + 'M';
        if (n >= 1000) return (n / 1000).toFixed(1) + 'K';
        return String(n);
    }

    function loadUsage(keyId) {
        var statsGrid = document.getElementById('usage-stats-grid');
        var chartEl = document.getElementById('usage-chart');
        var chartCard = document.getElementById('usage-chart-card');
        var emptyEl = document.getElementById('usage-empty');

        statsGrid.innerHTML = '';
        chartEl.innerHTML = '';

        Dashboard.api.get('/api/dashboard/keys/' + keyId + '/usage')
            .then(function(data) {
                var s = data.summary;
                var daily = data.daily || [];

                if (s.total_requests === 0) {
                    statsGrid.innerHTML = '';
                    chartCard.classList.add('hidden');
                    emptyEl.classList.remove('hidden');
                    renderStatCards(statsGrid, s);
                    return;
                }

                emptyEl.classList.add('hidden');
                chartCard.classList.remove('hidden');
                renderStatCards(statsGrid, s);
                renderChart(chartEl, daily);
            })
            .catch(function() {
                emptyEl.classList.remove('hidden');
                chartCard.classList.add('hidden');
            });
    }

    function renderStatCards(container, s) {
        container.innerHTML =
            statCard('Total Requests', fmtNumber(s.total_requests), '') +
            statCard('Prompt Tokens', fmtNumber(s.prompt_tokens), 'Input') +
            statCard('Completion Tokens', fmtNumber(s.completion_tokens), 'Output') +
            statCard('Total Tokens', fmtNumber(s.total_tokens),
                     s.total_requests > 0
                        ? '~' + Math.round(s.total_tokens / s.total_requests) + ' avg/req'
                        : '');
    }

    function statCard(label, value, sub) {
        return '<div class="usage-stat-card">' +
            '<div class="usage-stat-label">' + esc(label) + '</div>' +
            '<div class="usage-stat-value">' + esc(value) + '</div>' +
            (sub ? '<div class="usage-stat-sub">' + esc(sub) + '</div>' : '') +
        '</div>';
    }

    function renderChart(container, daily) {
        if (!daily.length) return;

        var maxTokens = 1;
        for (var i = 0; i < daily.length; i++) {
            var total = daily[i].prompt_tokens + daily[i].completion_tokens;
            if (total > maxTokens) maxTokens = total;
        }

        var html = '';
        for (var j = 0; j < daily.length; j++) {
            var d = daily[j];
            var pHeight = Math.max(2, (d.prompt_tokens / maxTokens) * 116);
            var cHeight = Math.max(2, (d.completion_tokens / maxTokens) * 116);
            var dateLabel = d.date.substring(5); /* MM-DD */

            html += '<div class="usage-bar-group">' +
                '<div class="usage-bar-stack">' +
                    '<div class="usage-bar usage-bar-completion" style="height:' +
                        cHeight + 'px" title="Completion: ' + d.completion_tokens + '"></div>' +
                    '<div class="usage-bar usage-bar-prompt" style="height:' +
                        pHeight + 'px" title="Prompt: ' + d.prompt_tokens + '"></div>' +
                '</div>' +
                '<span class="usage-bar-label">' + dateLabel + '</span>' +
            '</div>';
        }
        container.innerHTML = html;
    }

    /* --- Create / Edit Modal --- */

    function openCreateModal() {
        editingKeyId = null;
        document.getElementById('modal-key-title').textContent = 'Create API Key';
        document.getElementById('modal-key-submit').textContent = 'Create';
        resetForm();
        document.getElementById('modal-key').classList.remove('hidden');
    }

    function openEditModal(k) {
        editingKeyId = k.id;
        document.getElementById('modal-key-title').textContent = 'Edit API Key';
        document.getElementById('modal-key-submit').textContent = 'Save';
        document.getElementById('key-name').value = k.name || '';
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
