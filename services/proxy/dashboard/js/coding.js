/* Cloud Coding -- VPS + Docker + GitHub + Async Tasks */
var Dashboard = Dashboard || {};

Dashboard.coding = (function() {
    var projects = [];
    var currentProject = null;
    var ws = null;
    var term = null;
    var fitAddon = null;
    var resizeTimer = null;
    var statusPollTimer = null;
    var githubStatus = { connected: false, login: '' };

    var els = {};

    function mount() {
        els.sidebar = document.getElementById('coding-project-list');
        els.mainTitle = document.getElementById('coding-main-title');
        els.modelSelect = document.getElementById('coding-model-select');
        els.statusDot = document.getElementById('coding-status-dot');
        els.statusText = document.getElementById('coding-status-text');
        els.btnReconnect = document.getElementById('coding-btn-reconnect');
        els.btnNewProject = document.getElementById('coding-btn-new-project');
        els.termContainer = document.getElementById('coding-terminal');
        els.noProject = document.getElementById('coding-no-project');
        els.githubSection = document.getElementById('coding-github-section');
        els.githubBtn = document.getElementById('coding-github-btn');
        els.githubUser = document.getElementById('coding-github-user');
        els.taskInfo = document.getElementById('coding-task-info');
        els.btnLogs = document.getElementById('coding-btn-logs');
        els.logsPanel = document.getElementById('coding-logs-panel');
        els.logsContent = document.getElementById('coding-logs-content');

        /* Modal elements */
        els.modalOverlay = document.getElementById('coding-modal-overlay');
        els.modalRepoSelect = document.getElementById('coding-proj-repo');
        els.modalModel = document.getElementById('coding-proj-model');
        els.modalInstructions = document.getElementById('coding-proj-instructions');
        els.modalCancel = document.getElementById('coding-modal-cancel');
        els.modalSubmit = document.getElementById('coding-modal-submit');

        Dashboard.models.populateSelect(els.modelSelect, 'gpt-4.1');
        Dashboard.models.populateSelect(els.modalModel, 'gpt-4.1');

        els.btnNewProject.addEventListener('click', showNewProjectModal);
        els.btnReconnect.addEventListener('click', function() {
            if (currentProject) connectTerminal(currentProject.id);
        });
        if (els.btnLogs)
            els.btnLogs.addEventListener('click', toggleLogs);
        els.modelSelect.addEventListener('change', onModelChange);
        els.modalCancel.addEventListener('click', hideModal);
        els.modalSubmit.addEventListener('click', createProject);
        els.modalOverlay.addEventListener('click', function(e) {
            if (e.target === els.modalOverlay) hideModal();
        });
        if (els.githubBtn)
            els.githubBtn.addEventListener('click', onGitHubClick);

        window.addEventListener('resize', onWindowResize);

        loadGitHubStatus();
        loadProjects();
    }

    function unmount() {
        disconnectWs();
        disposeTerm();
        window.removeEventListener('resize', onWindowResize);
        if (resizeTimer) { clearTimeout(resizeTimer); resizeTimer = null; }
        if (statusPollTimer) {
            clearInterval(statusPollTimer);
            statusPollTimer = null;
        }
    }

    /* ---- GitHub ---- */

    function loadGitHubStatus() {
        Dashboard.api.get('/api/dashboard/github/status')
            .then(function(data) {
                githubStatus = data;
                renderGitHub();
            })
            .catch(function() {
                githubStatus = { connected: false };
                renderGitHub();
            });
    }

    function renderGitHub() {
        if (!els.githubSection) return;
        if (githubStatus.connected) {
            els.githubBtn.textContent = 'Disconnect';
            els.githubBtn.className = 'coding-github-btn disconnect';
            els.githubUser.textContent = githubStatus.login || 'Connected';
            els.githubUser.classList.remove('hidden');
        } else {
            els.githubBtn.textContent = 'Connect GitHub';
            els.githubBtn.className = 'coding-github-btn connect';
            els.githubUser.classList.add('hidden');
        }
    }

    function onGitHubClick() {
        if (githubStatus.connected) {
            if (!confirm('Disconnect GitHub? Running tasks will not be affected.'))
                return;
            Dashboard.api.post('/api/dashboard/github/disconnect')
                .then(function() {
                    githubStatus = { connected: false };
                    renderGitHub();
                });
        } else {
            /* Redirect to GitHub OAuth, passing dashboard token as state */
            var token = Dashboard.api.getToken();
            var url = '/api/dashboard/github/auth?state=' +
                encodeURIComponent(token);
            window.location.href = url;
        }
    }

    /* ---- Projects API ---- */

    function loadProjects() {
        Dashboard.api.get('/api/dashboard/projects')
            .then(function(data) {
                projects = data;
                renderProjectList();
                if (projects.length > 0 && !currentProject)
                    selectProject(projects[0].id);
                startStatusPolling();
            })
            .catch(function(err) {
                console.error('Failed to load projects:', err);
            });
    }

    function createProject() {
        var repo = els.modalRepoSelect.value;
        if (!repo) { els.modalRepoSelect.focus(); return; }

        var body = {
            repo_full_name: repo,
            name: repo.split('/').pop(),
            model: els.modalModel.value || 'gpt-4.1',
            instructions: els.modalInstructions.value.trim()
        };

        els.modalSubmit.disabled = true;
        els.modalSubmit.textContent = 'Starting...';

        Dashboard.api.post('/api/dashboard/projects', body)
            .then(function(proj) {
                hideModal();
                projects.unshift(proj);
                renderProjectList();
                selectProject(proj.id);
            })
            .catch(function(err) {
                alert('Failed to create project: ' + err.message);
            })
            .finally(function() {
                els.modalSubmit.disabled = false;
                els.modalSubmit.textContent = 'Start Task';
            });
    }

    function deleteProject(id) {
        if (!confirm('Delete this task? Container will be removed.'))
            return;

        Dashboard.api.del('/api/dashboard/projects/' + id)
            .then(function() {
                projects = projects.filter(function(p) { return p.id !== id; });
                if (currentProject && currentProject.id === id) {
                    disconnectWs();
                    disposeTerm();
                    currentProject = null;
                    showNoProject();
                }
                renderProjectList();
                if (!currentProject && projects.length > 0)
                    selectProject(projects[0].id);
            })
            .catch(function(err) {
                alert('Failed to delete: ' + err.message);
            });
    }

    function selectProject(id) {
        var proj = projects.find(function(p) { return p.id === id; });
        if (!proj) return;

        currentProject = proj;
        renderProjectList();
        updateTaskInfo(proj);

        if (els.modelSelect.querySelector('option[value="' + proj.model + '"]'))
            els.modelSelect.value = proj.model;

        if (els.noProject) els.noProject.classList.add('hidden');
        if (els.logsPanel) els.logsPanel.classList.add('hidden');

        if (proj.status === 'running' && proj.container_id) {
            els.termContainer.classList.remove('hidden');
            connectTerminal(id);
        } else {
            els.termContainer.classList.add('hidden');
        }
    }

    function onModelChange() {
        if (!currentProject) return;
        var newModel = els.modelSelect.value;
        if (newModel === currentProject.model) return;
        currentProject.model = newModel;
        Dashboard.api.put('/api/dashboard/projects/' + currentProject.id, {
            model: newModel
        });
        var p = projects.find(function(x) { return x.id === currentProject.id; });
        if (p) p.model = newModel;
        renderProjectList();
    }

    /* ---- Status Polling ---- */

    function startStatusPolling() {
        if (statusPollTimer) clearInterval(statusPollTimer);
        statusPollTimer = setInterval(pollRunningTasks, 10000);
    }

    function pollRunningTasks() {
        var running = projects.filter(function(p) {
            return p.status === 'running';
        });
        running.forEach(function(p) {
            Dashboard.api.get('/api/dashboard/projects/' + p.id + '/status')
                .then(function(data) {
                    var idx = projects.findIndex(function(x) {
                        return x.id === p.id;
                    });
                    if (idx >= 0) {
                        projects[idx] = data;
                        if (currentProject && currentProject.id === p.id)
                            currentProject = data;
                    }
                    renderProjectList();
                    if (currentProject && currentProject.id === p.id)
                        updateTaskInfo(data);
                })
                .catch(function() {});
        });
    }

    /* ---- Terminal WebSocket ---- */

    function connectTerminal(projectId) {
        disconnectWs();
        disposeTerm();
        setStatus('connecting', 'Connecting...');

        term = new Terminal({
            cursorBlink: true,
            fontSize: 14,
            fontFamily: '"SF Mono", "Fira Code", "Cascadia Code", monospace',
            theme: {
                background: '#1e1e1e',
                foreground: '#d4d4d4',
                cursor: '#d4d4d4',
                selectionBackground: '#264f78'
            },
            allowProposedApi: true
        });
        fitAddon = new FitAddon.FitAddon();
        term.loadAddon(fitAddon);
        term.open(els.termContainer);
        fitAddon.fit();

        var proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
        var token = Dashboard.api.getToken();
        var url = proto + '//' + location.host +
            '/api/dashboard/terminal?project_id=' + projectId +
            '&token=' + encodeURIComponent(token);

        ws = new WebSocket(url);
        ws.binaryType = 'arraybuffer';

        ws.onopen = function() {
            setStatus('connected', 'Connected');
            ws.send(JSON.stringify({
                type: 'resize', cols: term.cols, rows: term.rows
            }));
        };

        ws.onmessage = function(e) {
            if (e.data instanceof ArrayBuffer)
                term.write(new Uint8Array(e.data));
            else
                term.write(e.data);
        };

        ws.onclose = function() { setStatus('disconnected', 'Disconnected'); };
        ws.onerror = function() { setStatus('disconnected', 'Connection error'); };

        term.onData(function(data) {
            if (ws && ws.readyState === WebSocket.OPEN)
                ws.send(data);
        });
    }

    function disconnectWs() {
        if (ws) {
            ws.onclose = null; ws.onerror = null; ws.onmessage = null;
            if (ws.readyState <= 1) ws.close();
            ws = null;
        }
    }

    function disposeTerm() {
        if (term) { term.dispose(); term = null; fitAddon = null; }
    }

    /* ---- Logs ---- */

    function toggleLogs() {
        if (!currentProject || !els.logsPanel) return;
        if (els.logsPanel.classList.contains('hidden')) {
            els.logsPanel.classList.remove('hidden');
            loadLogs();
        } else {
            els.logsPanel.classList.add('hidden');
        }
    }

    function loadLogs() {
        if (!currentProject) return;
        Dashboard.api.get('/api/dashboard/projects/' + currentProject.id + '/logs')
            .then(function(data) {
                if (els.logsContent)
                    els.logsContent.textContent = data.logs || '(no logs)';
            })
            .catch(function() {
                if (els.logsContent)
                    els.logsContent.textContent = '(failed to load logs)';
            });
    }

    /* ---- UI Helpers ---- */

    function renderProjectList() {
        els.sidebar.innerHTML = '';
        if (projects.length === 0) {
            els.sidebar.innerHTML =
                '<div class="coding-empty-projects">' +
                'No tasks yet.<br>Connect GitHub and start a task.</div>';
            return;
        }

        for (var i = 0; i < projects.length; i++) {
            var p = projects[i];
            var isActive = currentProject && currentProject.id === p.id;

            var item = document.createElement('div');
            item.className = 'coding-project-item' + (isActive ? ' active' : '');
            item.setAttribute('data-id', p.id);

            var statusDot = document.createElement('span');
            statusDot.className = 'coding-task-status-dot ' + (p.status || 'pending');
            statusDot.title = p.status || 'pending';

            var name = document.createElement('span');
            name.className = 'coding-project-name';
            name.textContent = p.name || p.repo_full_name;

            var model = document.createElement('span');
            model.className = 'coding-project-model';
            model.textContent = (p.model || '').replace('gpt-', '');

            var actions = document.createElement('span');
            actions.className = 'coding-project-actions';
            var delBtn = document.createElement('button');
            delBtn.className = 'coding-project-action-btn';
            delBtn.title = 'Delete task';
            delBtn.innerHTML = '<span class="material-symbols-outlined">delete</span>';
            delBtn.setAttribute('data-delete-id', p.id);
            actions.appendChild(delBtn);

            item.appendChild(statusDot);
            item.appendChild(name);
            item.appendChild(model);
            item.appendChild(actions);
            els.sidebar.appendChild(item);
        }

        els.sidebar.onclick = function(e) {
            var delBtn = e.target.closest('[data-delete-id]');
            if (delBtn) {
                e.stopPropagation();
                deleteProject(parseInt(delBtn.getAttribute('data-delete-id'), 10));
                return;
            }
            var item = e.target.closest('.coding-project-item');
            if (item)
                selectProject(parseInt(item.getAttribute('data-id'), 10));
        };
    }

    function updateTaskInfo(proj) {
        els.mainTitle.textContent = proj.name || proj.repo_full_name || 'Cloud Coding';
        if (!els.taskInfo) return;

        var parts = [];
        if (proj.status) parts.push('Status: ' + proj.status);
        if (proj.branch_name) parts.push('Branch: ' + proj.branch_name);
        if (proj.repo_full_name) parts.push('Repo: ' + proj.repo_full_name);
        els.taskInfo.textContent = parts.join('  |  ');

        /* Show/hide action buttons */
        if (els.btnReconnect)
            els.btnReconnect.style.display =
                (proj.status === 'running') ? '' : 'none';
        if (els.btnLogs)
            els.btnLogs.style.display =
                (proj.container_id) ? '' : 'none';
    }

    function showNoProject() {
        els.termContainer.classList.add('hidden');
        if (els.noProject) els.noProject.classList.remove('hidden');
        els.mainTitle.textContent = 'Cloud Coding';
        setStatus('', '');
    }

    function setStatus(state, text) {
        els.statusDot.className = 'coding-status-dot' +
            (state ? ' ' + state : '');
        els.statusText.textContent = text;
    }

    function showNewProjectModal() {
        if (!githubStatus.connected) {
            alert('Connect your GitHub account first.');
            return;
        }
        els.modalInstructions.value = '';
        els.modalOverlay.classList.remove('hidden');
        /* Load repos into dropdown */
        loadReposIntoModal();
    }

    function loadReposIntoModal() {
        els.modalRepoSelect.innerHTML = '<option value="">Loading repos...</option>';
        Dashboard.api.get('/api/dashboard/github/repos')
            .then(function(repos) {
                els.modalRepoSelect.innerHTML = '<option value="">Select a repo</option>';
                repos.forEach(function(r) {
                    var opt = document.createElement('option');
                    opt.value = r.full_name;
                    opt.textContent = r.full_name +
                        (r.private ? ' (private)' : '');
                    els.modalRepoSelect.appendChild(opt);
                });
            })
            .catch(function() {
                els.modalRepoSelect.innerHTML =
                    '<option value="">Failed to load repos</option>';
            });
    }

    function hideModal() {
        els.modalOverlay.classList.add('hidden');
    }

    function onWindowResize() {
        if (resizeTimer) clearTimeout(resizeTimer);
        resizeTimer = setTimeout(function() {
            if (fitAddon && term) {
                fitAddon.fit();
                if (ws && ws.readyState === WebSocket.OPEN)
                    ws.send(JSON.stringify({
                        type: 'resize', cols: term.cols, rows: term.rows
                    }));
            }
        }, 150);
    }

    return { mount: mount, unmount: unmount };
})();
