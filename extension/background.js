const UVA_DOMAIN = "aichat.uva.nl";
const TARGET_COOKIES = ["__Host-authjs.csrf-token", "authjs.session-token"];

let syncTimer = null;

// Debounced sync -- multiple cookies change during login
function scheduleSyncDebounced() {
  if (syncTimer) clearTimeout(syncTimer);
  syncTimer = setTimeout(() => {
    syncTimer = null;
    syncCookies();
  }, 2000);
}

// Listen for cookie changes on aichat.uva.nl
chrome.cookies.onChanged.addListener((info) => {
  if (info.cookie.domain === UVA_DOMAIN || info.cookie.domain === "." + UVA_DOMAIN) {
    if (TARGET_COOKIES.includes(info.cookie.name)) {
      scheduleSyncDebounced();
    }
  }
});

// Also sync on install and startup
chrome.runtime.onInstalled.addListener(() => syncCookies());
chrome.runtime.onStartup.addListener(() => syncCookies());

// Manual sync from popup
chrome.runtime.onMessage.addListener((msg, _sender, sendResponse) => {
  if (msg.type === "sync_now") {
    syncCookies().then((result) => sendResponse(result));
    return true; // keep channel open for async response
  }
});

async function getConfig() {
  const result = await chrome.storage.local.get(["proxyUrl", "apiKey"]);
  return {
    proxyUrl: result.proxyUrl || "",
    apiKey: result.apiKey || "",
  };
}

async function syncCookies() {
  const config = await getConfig();

  if (!config.proxyUrl || !config.apiKey) {
    setBadge("?", "#FFA500");
    const msg = "Extension not configured. Open the popup to set proxy URL and API key.";
    await saveStatus(false, msg);
    return { ok: false, message: msg };
  }

  try {
    const cookies = await chrome.cookies.getAll({ domain: UVA_DOMAIN });
    if (!cookies || cookies.length === 0) {
      setBadge("!", "#FF4444");
      const msg = "No cookies found for " + UVA_DOMAIN + ". Log in first.";
      await saveStatus(false, msg);
      return { ok: false, message: msg };
    }

    const cookieString = cookies
      .map((c) => c.name + "=" + c.value)
      .join("; ");

    const url = config.proxyUrl.replace(/\/+$/, "") + "/api/v1/session";

    const res = await fetch(url, {
      method: "PUT",
      headers: {
        "Content-Type": "application/json",
        "Authorization": "Bearer " + config.apiKey,
      },
      body: JSON.stringify({ uva_session: cookieString }),
    });

    if (!res.ok) {
      const errBody = await res.json().catch(() => ({}));
      const errMsg = errBody.error || "HTTP " + res.status;
      setBadge("!", "#FF4444");
      await saveStatus(false, errMsg);
      return { ok: false, message: errMsg };
    }

    setBadge("", "#44BB44");
    const msg = "Synced successfully";
    await saveStatus(true, msg);
    return { ok: true, message: msg };
  } catch (err) {
    const errMsg = err.message || "Network error";
    setBadge("!", "#FF4444");
    await saveStatus(false, errMsg);
    return { ok: false, message: errMsg };
  }
}

function setBadge(text, color) {
  chrome.action.setBadgeText({ text });
  chrome.action.setBadgeBackgroundColor({ color });
}

async function saveStatus(ok, message) {
  await chrome.storage.local.set({
    lastSync: {
      ok,
      message,
      timestamp: new Date().toISOString(),
    },
  });
}
