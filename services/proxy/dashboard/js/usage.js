/* API key usage modal */
var Dashboard = Dashboard || {};

Dashboard.usage = (function() {
    function fmtNum(n) {
        if (n === null || n === undefined) return '-';
        if (n >= 1000000) return (n / 1000000).toFixed(1) + 'M';
        if (n >= 1000) return (n / 1000).toFixed(1) + 'K';
        return String(n);
    }

    function esc(str) {
        var div = document.createElement('div');
        div.textContent = str;
        return div.innerHTML;
    }

    function open(k) {
        document.getElementById('modal-usage-title').textContent =
            esc(k.name || 'Unnamed Key');
        document.getElementById('modal-usage-subtitle').textContent =
            k.key_prefix;
        document.getElementById('us-requests').textContent = '-';
        document.getElementById('us-input').textContent = '-';
        document.getElementById('us-output').textContent = '-';
        document.getElementById('us-total').textContent = '-';
        document.getElementById('us-errors').textContent = '-';
        document.getElementById('us-latency').textContent = '-';
        document.getElementById('usage-chart').innerHTML = '';
        document.getElementById('usage-no-data').classList.add('hidden');
        document.getElementById('modal-usage').classList.remove('hidden');

        Dashboard.api.get('/api/dashboard/keys/' + k.id + '/usage')
            .then(function(data) { render(data); })
            .catch(function(err) {
                document.getElementById('us-requests').textContent = 'Error';
                console.error('Usage fetch failed:', err);
            });
    }

    function render(data) {
        document.getElementById('us-requests').textContent =
            fmtNum(data.total_requests);
        document.getElementById('us-input').textContent =
            fmtNum(data.total_input_tokens);
        document.getElementById('us-output').textContent =
            fmtNum(data.total_output_tokens);
        document.getElementById('us-total').textContent =
            fmtNum(data.total_tokens);
        document.getElementById('us-errors').textContent =
            fmtNum(data.error_requests);
        document.getElementById('us-latency').textContent =
            data.avg_latency_ms > 0 ? data.avg_latency_ms + ' ms' : '-';

        var daily = data.daily || [];
        var chart = document.getElementById('usage-chart');
        chart.innerHTML = '';

        if (daily.length === 0) {
            document.getElementById('usage-no-data').classList.remove('hidden');
            return;
        }

        var maxReqs = 1;
        for (var i = 0; i < daily.length; i++)
            if (daily[i].requests > maxReqs) maxReqs = daily[i].requests;

        /* daily is newest-first; reverse for left-to-right display */
        var ordered = daily.slice().reverse();
        for (var j = 0; j < ordered.length; j++) {
            var d = ordered[j];
            var pct = Math.max(4, Math.round((d.requests / maxReqs) * 100));
            var wrap = document.createElement('div');
            wrap.className = 'usage-bar-wrap';
            wrap.title = d.day + ': ' + d.requests + ' req, ' +
                fmtNum(d.input_tokens) + ' in, ' +
                fmtNum(d.output_tokens) + ' out';
            var bar = document.createElement('div');
            bar.className = 'usage-bar';
            bar.style.height = pct + '%';
            var label = document.createElement('div');
            label.className = 'usage-bar-day';
            label.textContent = d.day.slice(5); /* MM-DD */
            wrap.appendChild(bar);
            wrap.appendChild(label);
            chart.appendChild(wrap);
        }
    }

    function close() {
        document.getElementById('modal-usage').classList.add('hidden');
    }

    return { open: open, close: close };
})();
