const proxyUrlInput = document.getElementById("proxyUrl");
const apiKeyInput = document.getElementById("apiKey");
const saveBtn = document.getElementById("saveBtn");
const syncBtn = document.getElementById("syncBtn");
const statusEl = document.getElementById("status");

// Load saved config and status on popup open
chrome.storage.local.get(["proxyUrl", "apiKey", "lastSync"], (data) => {
  proxyUrlInput.value = data.proxyUrl || "";
  apiKeyInput.value = data.apiKey || "";
  if (data.lastSync) showStatus(data.lastSync);
});

saveBtn.addEventListener("click", () => {
  const proxyUrl = proxyUrlInput.value.trim();
  const apiKey = apiKeyInput.value.trim();

  if (!proxyUrl) {
    showStatus({ ok: false, message: "Proxy URL is required." });
    return;
  }
  if (!apiKey.startsWith("uva-")) {
    showStatus({ ok: false, message: "API key must start with 'uva-'." });
    return;
  }

  chrome.storage.local.set({ proxyUrl, apiKey }, () => {
    showStatus({ ok: true, message: "Configuration saved." });
  });
});

syncBtn.addEventListener("click", () => {
  syncBtn.disabled = true;
  syncBtn.textContent = "Syncing...";

  chrome.runtime.sendMessage({ type: "sync_now" }, (result) => {
    syncBtn.disabled = false;
    syncBtn.textContent = "Sync Now";
    if (result) {
      showStatus({
        ok: result.ok,
        message: result.message,
        timestamp: new Date().toISOString(),
      });
    }
  });
});

function showStatus(info) {
  const ok = info.ok;
  const time = info.timestamp
    ? new Date(info.timestamp).toLocaleTimeString()
    : "";

  statusEl.className = "status " + (ok ? "status-ok" : "status-err");
  statusEl.textContent = info.message + (time ? " (" + time + ")" : "");
}
