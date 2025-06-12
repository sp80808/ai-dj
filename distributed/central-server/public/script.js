const API_BASE = "/api/v1";
function showRegistrationForm() {
  document.getElementById("registrationForm").classList.remove("hidden");
  document.getElementById("loginForm").classList.add("hidden");
  document.getElementById("verificationSuccess").classList.add("hidden");
  document.getElementById("userDashboard").classList.add("hidden");
}
function showLoginForm() {
  document.getElementById("registrationForm").classList.add("hidden");
  document.getElementById("loginForm").classList.remove("hidden");
  document.getElementById("verificationSuccess").classList.add("hidden");
  document.getElementById("userDashboard").classList.add("hidden");
}
function showVerificationSuccess() {
  document.getElementById("registrationForm").classList.add("hidden");
  document.getElementById("loginForm").classList.add("hidden");
  document.getElementById("verificationSuccess").classList.remove("hidden");
  document.getElementById("userDashboard").classList.add("hidden");
}
// Variable globale pour stocker les données utilisateur
let currentUserData = null;

function showDashboard(userData) {
  // Stocker les données utilisateur
  currentUserData = userData;

  document.getElementById("registrationForm").classList.add("hidden");
  document.getElementById("loginForm").classList.add("hidden");
  document.getElementById("verificationSuccess").classList.add("hidden");
  document.getElementById("userDashboard").classList.remove("hidden");
  document.getElementById("apiKeysManagement").classList.add("hidden"); // S'assurer que les clés API sont cachées

  document.getElementById(
    "userInfo"
  ).textContent = `Hello ${userData.username}! (${userData.email})`;
  document.getElementById("creditsInfo").textContent = `Daily generations: ${
    userData.daily_generations_used || 0
  }/${userData.daily_generations_limit || 25}`;

  const dashboardDiv = document.getElementById("userDashboard");
  const apiButton =
    '<button class="btn" onclick="showApiKeys()" style="margin-top: 1rem; background: #9c27b0;">🔑 Manage API Keys</button>';

  if (!dashboardDiv.innerHTML.includes("Manage API Keys")) {
    dashboardDiv.innerHTML = dashboardDiv.innerHTML.replace(
      '<button class="btn" onclick="logout()"',
      apiButton + '<button class="btn" onclick="logout()"'
    );
  }
}

function showApiKeys() {
  document
    .getElementById("userDashboard")
    .querySelector(".verification-info").style.display = "none";
  document
    .getElementById("userDashboard")
    .querySelector('button[onclick="logout()"]').style.display = "none";
  document
    .getElementById("userDashboard")
    .querySelector('button[onclick="showApiKeys()"]').style.display = "none";
  document.getElementById("apiKeysManagement").classList.remove("hidden");
  loadApiKeys();
}

function backToDashboard() {
  document.getElementById("apiKeysManagement").classList.add("hidden");
  document
    .getElementById("userDashboard")
    .querySelector(".verification-info").style.display = "block";
  document
    .getElementById("userDashboard")
    .querySelector('button[onclick="logout()"]').style.display = "block";
  document
    .getElementById("userDashboard")
    .querySelector('button[onclick="showApiKeys()"]').style.display = "block";
}

async function revokeApiKey(keyId) {
  if (
    !confirm(
      "Are you sure you want to revoke this API key? This action cannot be undone."
    )
  ) {
    return;
  }

  const token = localStorage.getItem("access_token");

  try {
    const response = await fetch(`/api/v1/user/api-keys/${keyId}`, {
      method: "DELETE",
      headers: {
        Authorization: `Bearer ${token}`,
      },
    });

    const result = await response.json();

    if (result.success) {
      alert("API key revoked successfully");
      loadApiKeys();
    } else {
      alert("Failed to revoke API key: " + result.error.message);
    }
  } catch (error) {
    alert("Network error revoking API key");
  }
}
function showAlert(containerId, message, type = "error") {
  const container = document.getElementById(containerId);
  container.innerHTML = `<div class="alert alert-${type}">${message}</div>`;
}

function clearAlert(containerId) {
  document.getElementById(containerId).innerHTML = "";
}

function validatePasswordRealTime(password) {
  const requirements = [
    { id: "req-length", test: password.length >= 8 },
    { id: "req-uppercase", test: /[A-Z]/.test(password) },
    { id: "req-lowercase", test: /[a-z]/.test(password) },
    { id: "req-number", test: /\d/.test(password) },
    { id: "req-special", test: /[!@#$%^&*(),.?":{}|<>]/.test(password) },
  ];

  let allValid = true;

  requirements.forEach((req) => {
    const element = document.getElementById(req.id);
    const icon = element.querySelector(".requirement-icon");

    if (req.test) {
      element.classList.remove("invalid");
      element.classList.add("valid");
      icon.textContent = "✓";
    } else {
      element.classList.remove("valid");
      element.classList.add("invalid");
      icon.textContent = "○";
      allValid = false;
    }
  });

  const passwordIcon = document.getElementById("passwordValidationIcon");
  if (password.length === 0) {
    passwordIcon.textContent = "";
  } else if (allValid) {
    passwordIcon.innerHTML = '<span class="valid-icon">✓</span>';
  } else {
    passwordIcon.innerHTML = '<span class="invalid-icon">⚠</span>';
  }

  return allValid;
}

function validateEmailRealTime(email) {
  const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
  const isValid = emailRegex.test(email);

  const emailIcon = document.getElementById("emailValidationIcon");

  if (email.length === 0) {
    emailIcon.textContent = "";
  } else if (isValid) {
    emailIcon.innerHTML = '<span class="valid-icon">✓</span>';
  } else {
    emailIcon.innerHTML = '<span class="invalid-icon">⚠</span>';
  }

  return isValid;
}

function validateUsernameRealTime(username) {
  const isValid = username.length >= 3 && /^[a-zA-Z0-9_-]+$/.test(username);

  const usernameIcon = document.getElementById("usernameValidationIcon");

  if (!usernameIcon) {
    console.warn("usernameValidationIcon element not found");
    return isValid;
  }

  if (username.length === 0) {
    usernameIcon.textContent = "";
  } else if (isValid) {
    usernameIcon.innerHTML = '<span class="valid-icon">✓</span>';
  } else {
    usernameIcon.innerHTML = '<span class="invalid-icon">⚠</span>';
  }

  return isValid;
}

function validatePassword(password) {
  const minLength = 8;
  const hasUpperCase = /[A-Z]/.test(password);
  const hasLowerCase = /[a-z]/.test(password);
  const hasNumbers = /\d/.test(password);
  const hasSpecialChar = /[!@#$%^&*(),.?":{}|<>]/.test(password);

  if (password.length < minLength) {
    return "Password must be at least 8 characters long";
  }
  if (!hasUpperCase) {
    return "Password must contain at least one uppercase letter";
  }
  if (!hasLowerCase) {
    return "Password must contain at least one lowercase letter";
  }
  if (!hasNumbers) {
    return "Password must contain at least one number";
  }
  if (!hasSpecialChar) {
    return "Password must contain at least one special character";
  }

  return null;
}

document
  .getElementById("registerForm")
  .addEventListener("submit", async (e) => {
    e.preventDefault();
    const email = document.getElementById("email").value;
    const username = document.getElementById("username").value;
    const password = document.getElementById("password").value;

    const emailValid = validateEmailRealTime(email);
    const usernameValid = validateUsernameRealTime(username);
    const passwordValid = validatePasswordRealTime(password);

    if (!emailValid) {
      showAlert("alertContainer", "Please enter a valid email address");
      return;
    }

    if (!usernameValid) {
      showAlert(
        "alertContainer",
        "Username must be at least 3 characters and contain only letters, numbers, underscore and dash"
      );
      return;
    }

    if (!passwordValid) {
      showAlert("alertContainer", "Please meet all password requirements");
      return;
    }
    const btn = document.getElementById("registerBtn");
    btn.disabled = true;
    btn.classList.add("loading");
    btn.textContent = "";

    clearAlert("alertContainer");

    const formData = new FormData(e.target);
    const userData = Object.fromEntries(formData);

    try {
      const response = await fetch(`${API_BASE}/auth/register`, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify(userData),
      });

      const result = await response.json();

      if (result.success) {
        showVerificationSuccess();
      } else {
        showAlert("alertContainer", result.error.message);
      }
    } catch (error) {
      showAlert("alertContainer", "Network error. Please try again.");
    } finally {
      btn.disabled = false;
      btn.classList.remove("loading");
      btn.textContent = "Create Account";
    }
  });

async function loadApiKeys() {
  const token = localStorage.getItem("access_token");

  try {
    const response = await fetch("/api/v1/user/api-keys", {
      headers: {
        Authorization: `Bearer ${token}`,
      },
    });

    const result = await response.json();

    if (result.success) {
      displayApiKeys(result.data);
    }
  } catch (error) {
    console.error("Failed to load API keys:", error);
  }
}

async function generateNewApiKey() {
  const token = localStorage.getItem("access_token");
  const keyName = prompt("Enter a name for this API key:") || "VST Plugin Key";

  document.getElementById("generateKeyBtn").disabled = true;

  try {
    const response = await fetch("/api/v1/user/api-keys", {
      method: "POST",
      headers: {
        Authorization: `Bearer ${token}`,
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ key_name: keyName }),
    });

    const result = await response.json();

    if (result.success) {
      alert(
        `🔑 Your new API key (save it now, you won't see it again!):\n\n${result.data.key}`
      );
      loadApiKeys();
    } else {
      alert("Failed to generate API key: " + result.error.message);
    }
  } catch (error) {
    alert("Network error generating API key");
  } finally {
    document.getElementById("generateKeyBtn").disabled = false;
  }
}

function displayApiKeys(keys) {
  const container = document.getElementById("apiKeysList");

  if (keys.length === 0) {
    container.innerHTML =
      '<p style="color: #a0a3bd;">No API keys yet. Generate one to use with the VST plugin.</p>';
    return;
  }

  container.innerHTML = keys
    .map(
      (key) => `
        <div style="background: rgba(255,255,255,0.05); padding: 1rem; border-radius: 8px; margin: 0.5rem 0;">
            <div style="display: flex; justify-content: space-between; align-items: center;">
                <div>
                    <strong>${key.key_name}</strong><br>
                    <small style="color: #a0a3bd;">${key.key_prefix}</small><br>
                    <small style="color: #a0a3bd;">Last used: ${
                      key.last_used_at || "Never"
                    }</small>
                </div>
                <button onclick="revokeApiKey(${
                  key.id
                })" style="background: #ef4444; color: white; border: none; padding: 0.5rem 1rem; border-radius: 4px; cursor: pointer;">
                    Revoke
                </button>
            </div>
        </div>
    `
    )
    .join("");
}

document
  .getElementById("loginFormElement")
  .addEventListener("submit", async (e) => {
    e.preventDefault();

    const btn = document.getElementById("loginBtn");
    btn.disabled = true;
    btn.classList.add("loading");
    btn.textContent = "";

    clearAlert("loginAlertContainer");

    const formData = new FormData(e.target);
    const loginData = Object.fromEntries(formData);

    try {
      const response = await fetch(`${API_BASE}/auth/login`, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          email_or_username: loginData.emailOrUsername,
          password: loginData.password,
        }),
      });

      const result = await response.json();

      if (result.success) {
        localStorage.setItem("access_token", result.data.tokens.access_token);
        localStorage.setItem("refresh_token", result.data.tokens.refresh_token);

        showDashboard(result.data.user);
      } else {
        showAlert("loginAlertContainer", result.error.message);
      }
    } catch (error) {
      showAlert("loginAlertContainer", "Network error. Please try again.");
    } finally {
      btn.disabled = false;
      btn.classList.remove("loading");
      btn.textContent = "Sign In";
    }
  });

function logout() {
  localStorage.removeItem("access_token");
  localStorage.removeItem("refresh_token");
  showLoginForm();
}

window.addEventListener("load", async () => {
  const token = localStorage.getItem("access_token");
  if (token) {
    try {
      const response = await fetch(`${API_BASE}/auth/me`, {
        headers: {
          Authorization: `Bearer ${token}`,
        },
      });

      if (response.ok) {
        const result = await response.json();
        showDashboard(result.data.user);
        return;
      }
    } catch (error) {
      showLoginForm();
    }
  }

  showLoginForm();
});
