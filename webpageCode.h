// HTML page stored in PROGMEM (Improved Web Dashboard UI/UX) - Modified to include Factory Reset Button
const char webpageCode[] PROGMEM = R"===(
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Home Monitoring</title>
    <style>
        /* Modern Professional Color Palette */
        :root {
            --primary: #0a2463;
            --secondary: #3e92cc;
            --accent: #2dc653;
            --warning: #ffb400;
            --danger: #d64045;
            --dark: #1e1f26;
            --light: #f8f9fa;
            --gray-100: #f0f2f5;
            --gray-200: #e9ecef;
            --gray-300: #dee2e6;
            --gray-400: #ced4da;
            --gray-500: #adb5bd;
            --gray-600: #6c757d;
            --gray-700: #495057;
            --gray-800: #343a40;
            --text: #212529;
            --box-shadow: 0 2px 8px rgba(0, 0, 0, 0.12);
            --card-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
            --transition: all 0.2s ease;
        }

        * {
            margin: 0;
            padding: 0;
        }

        body {
            font-family: 'Roboto', 'Segoe UI', sans-serif;
            background-color: var(--gray-100);
            color: var(--text);
            line-height: 1.6;
        }

        .dashboard-container {
            display: grid;
            grid-template-columns: 240px 1fr;
            min-height: 90vh;
        }

        /* Sidebar */
        .sidebar {
            background-color: var(--primary);
            color: white;
            padding-top: 25px;
            position: fixed;
            height: 100vh;
            width: 240px;
            z-index: 100;
            box-shadow: var(--box-shadow);
        }

        .brand {
            display: flex;
            align-items: center;
            padding: 0 20px 20px;
            border-bottom: 1px solid rgba(255, 255, 255, 0.1);
        }

        .brand-icon {
            font-size: 24px;
            margin-right: 10px;
        }

        .brand-name {
            font-size: 18px;
            font-weight: 600;
        }

        .nav-menu {
            list-style: none;
            padding: 20px 0;
        }

        .nav-item {
            padding: 0 15px;
            margin-bottom: 5px;
        }

        .nav-link {
            display: flex;
            align-items: center;
            padding: 12px 15px;
            color: rgba(255, 255, 255, 0.8);
            text-decoration: none;
            border-radius: 5px;
            transition: var(--transition);
        }

        .nav-link:hover {
            background-color: rgba(255, 255, 255, 0.1);
            color: white;
        }

        .nav-link.active {
            background-color: var(--secondary);
            color: white;
        }

        .nav-icon {
            margin-right: 10px;
            width: 20px;
            text-align: center;
        }

        /* Main Content */
        .main-content {
            grid-column: 2;
            padding: 20px;
        }

        .page-header {
            padding: 15px 0;
            margin-bottom: 20px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .page-title {
            font-size: 24px;
            font-weight: 500;
            color: var(--dark);
        }

        .page-subtitle {
            font-size: 14px;
            color: var(--gray-600);
            margin-top: 5px;
        }

        /* Cards & Grids */
        .card-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
            gap: 20px;
            margin-bottom: 25px;
        }

        .card {
            background-color: white;
            border-radius: 10px;
            box-shadow: var(--card-shadow);
            padding: 20px;
            transition: var(--transition);
        }

        .card:hover {
            box-shadow: 0 10px 15px rgba(0, 0, 0, 0.07);
            transform: translateY(-2px);
        }

        .card-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 15px;
        }

        .card-title {
            font-size: 16px;
            font-weight: 500;
            color: var(--gray-700);
        }

        .card-icon {
            font-size: 18px;
            width: 36px;
            height: 36px;
            display: flex;
            align-items: center;
            justify-content: center;
            border-radius: 50%;
            background-color: var(--secondary);
            color: white;
        }

        .dashboard-value {
            font-size: 32px;
            font-weight: 600;
            color: var(--primary);
            margin: 10px 0;
        }

        .unit {
            font-size: 16px;
            font-weight: normal;
            color: var(--gray-600);
        }

        .trend {
            display: flex;
            align-items: center;
            font-size: 14px;
            color: var(--gray-600);
        }

        .trend-up {
            color: var(--accent);
        }

        .trend-down {
            color: var(--danger);
        }

        /* Status Indicators */
        .status-card {
            display: flex;
            flex-direction: column;
            height: 100%;
        }

        .status-indicators {
            flex-grow: 1;
        }

        .status-item {
            display: flex;
            align-items: center;
            padding: 12px 0;
            border-bottom: 1px solid var(--gray-200);
        }

        .status-item:last-child {
            border-bottom: none;
        }

        .status-indicator {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 12px;
        }

        .status-active {
            background-color: var(--accent);
        }

        .status-inactive {
            background-color: var(--gray-500);
        }

        .status-warning {
            background-color: var(--warning);
        }

        .status-danger {
            background-color: var(--danger);
        }

        .status-label {
            font-weight: 500;
            flex-grow: 1;
        }

        .status-value {
            font-weight: 600;
        }

        /* Action Areas */
        .action-card {
            height: 100%;
        }

        .action-content {
            padding-top: 10px;
        }

        .input-group {
            display: flex;
            margin-bottom: 15px;
        }

        .firmware-version {
            font-size: 18px;
            color: rgb(0, 0, 0);
        }

        .input-group .form-input {
            flex-grow: 1;
            border-top-right-radius: 0;
            border-bottom-right-radius: 0;
        }

        .input-group .input-append {
            display: flex;
            align-items: center;
            padding: 0 12px;
            background-color: var(--gray-200);
            border: 1px solid var(--gray-300);
            border-left: none;
            border-top-right-radius: 5px;
            border-bottom-right-radius: 5px;
            color: var(--gray-700);
        }

        /* Forms */
        .form-group {
            margin-bottom: 20px;
        }

        .form-label {
            display: block;
            margin-bottom: 8px;
            font-weight: 500;
            color: var(--gray-700);
        }

        .form-input {
            width: 100%;
            padding: 10px 12px;
            border: 1px solid var(--gray-300);
            border-radius: 5px;
            font-size: 14px;
            transition: var(--transition);
        }

        .form-input:focus {
            outline: none;
            border-color: var(--secondary);
            box-shadow: 0 0 0 3px rgba(62, 146, 204, 0.1);
        }

        /* Buttons */
        .btn {
            display: inline-block;
            font-weight: 500;
            text-align: center;
            vertical-align: middle;
            cursor: pointer;
            border: 1px solid transparent;
            padding: 10px 16px;
            font-size: 14px;
            line-height: 1.5;
            border-radius: 5px;
            transition: var(--transition);
        }

        .btn-primary {
            color: white;
            background-color: var(--secondary);
            border-color: var(--secondary);
        }

        .btn-primary:hover {
            background-color: #3182b8;
            border-color: #3182b8;
        }

        .btn-success {
            color: white;
            background-color: var(--accent);
            border-color: var(--accent);
        }

        .btn-success:hover {
            background-color: #25a745;
            border-color: #25a745;
        }

        .btn-danger {
            color: white;
            background-color: var(--danger);
            border-color: var(--danger);
        }

        .btn-danger:hover {
            background-color: #c13c40;
            border-color: #c13c40;
        }

        .btn-block {
            display: block;
            width: 100%;
        }

        .btn:disabled {
            opacity: 0.65;
            pointer-events: none;
        }

        /* Alerts */
        .alert {
            padding: 12px 15px;
            margin-bottom: 15px;
            border-radius: 5px;
            border: 1px solid transparent;
            font-size: 14px;
        }

        .alert-success {
            color: #155724;
            background-color: #d4edda;
            border-color: #c3e6cb;
        }

        .alert-danger {
            color: #721c24;
            background-color: #f8d7da;
            border-color: #f5c6cb;
        }

        .alert-warning {
            color: #856404;
            background-color: #fff3cd;
            border-color: #ffeeba;
        }

        /* Settings */
        .settings-section {
            margin-bottom: 25px;
        }

        .settings-header {
            display: flex;
            align-items: center;
            margin-bottom: 15px;
            padding-bottom: 10px;
            border-bottom: 1px solid var(--gray-300);
        }

        .settings-icon {
            margin-right: 10px;
            color: var(--secondary);
        }

        .settings-title {
            font-size: 18px;
            font-weight: 500;
        }

        .settings-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
            gap: 20px;
        }

        /* Hide sections */
        .page-section {
            display: none;
        }

        .page-section.active {
            display: block;
        }

        /* ======== WIFI LOADER ANIMATION ======== */
        .loader {
            width: 40px;
            height: 40px;
            border: 4px solid #e9ecef;
            border-top: 4px solid var(--secondary);
            border-radius: 50%;
            animation: spin 1s linear infinite;
            margin: 0 auto;
        }

        @keyframes spin {
            0% {
                transform: rotate(0deg);
            }

            100% {
                transform: rotate(360deg);
            }
        }

        /* Status text styling */
        #wifiResult {
            padding: 8px;
            border-radius: 6px;
            text-align: center;
        }

        .success {
            background: #d4edda;
            color: #155724;
        }

        .fail {
            background: #f8d7da;
            color: #721c24;
        }

        /* Footer */
        .dashboard-footer {
            margin-top: 30px;
            padding-top: 15px;
            border-top: 1px solid var(--gray-300);
            color: var(--gray-600);
            font-size: 14px;
            text-align: center;
        }

        .progress-bar-animated {
            background: linear-gradient(45deg,
                    #3e92cc,
                    #2dc653,
                    #3e92cc);
            background-size: 200% 200%;
            animation: gradientMove 2s linear infinite;
        }

        @keyframes gradientMove {
            0% {
                background-position: 0% 50%;
            }

            50% {
                background-position: 100% 50%;
            }

            100% {
                background-position: 0% 50%;
            }
        }

        /* ================= MOBILE MENU ================= */
        .menu-toggle {
            display: none;
            position: fixed;
            top: 15px;
            left: 15px;
            z-index: 1000;
            background: var(--primary);
            color: white;
            border: none;
            padding: 8px 12px;
            font-size: 20px;
            border-radius: 6px;
            cursor: pointer;
            box-shadow: var(--box-shadow);
        }

        /* ================= MOBILE SIDEBAR TOGGLE ================= */
        .menu-toggle {
            display: none;
            position: fixed;
            top: 15px;
            left: 15px;
            z-index: 1000;
            background: var(--primary);
            border: none;
            border-radius: 8px;
            width: 44px;
            height: 44px;
            box-shadow: var(--box-shadow);
            cursor: pointer;
            align-items: center;
            justify-content: center;
        }

        /* Ikon burger custom (SVG) */
        .menu-toggle svg {
            width: 24px;
            height: 24px;
            stroke: white;
            stroke-width: 2;
            fill: none;
            transition: transform 0.3s ease;
        }

        /* Overlay gelap */
        .sidebar-overlay {
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0, 0, 0, 0.4);
            z-index: 90;
            opacity: 0;
            visibility: hidden;
            transition: all 0.3s ease;
        }

        /* Animasi slide sidebar */
        .sidebar {
            transition: transform 0.35s cubic-bezier(.17, .89, .32, 1.49);
            /* ease + bounce */
        }

        /* ============ FULLSCREEN WIFI MODAL ============ */
        .wifi-modal {
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(10, 36, 99, 0.9);
            /* Biru industrial */
            display: none;
            /* default hidden */
            align-items: center;
            justify-content: center;
            z-index: 9999;
            backdrop-filter: blur(5px);
        }

        .wifi-modal-content {
            text-align: center;
            color: white;
            background: rgba(255, 255, 255, 0.1);
            padding: 30px 40px;
            border-radius: 12px;
            box-shadow: 0 4px 20px rgba(0, 0, 0, 0.3);
        }

        /* Spinner utama */
        .wifi-spinner {
            width: 50px;
            height: 50px;
            border: 5px solid rgba(255, 255, 255, 0.3);
            border-top: 5px solid #2dc653;
            /* hijau sukses */
            border-radius: 50%;
            margin: 0 auto 15px auto;
            animation: spin 1s linear infinite;
        }

        @keyframes spin {
            0% {
                transform: rotate(0deg);
            }

            100% {
                transform: rotate(360deg);
            }
        }

        /* Animasi titik */
        .wifi-progress-dots {
            margin-top: 10px;
            font-size: 28px;
            letter-spacing: 5px;
        }

        .wifi-progress-dots span {
            animation: blink 1.5s infinite;
        }

        .wifi-progress-dots span:nth-child(1) {
            animation-delay: 0s;
        }

        .wifi-progress-dots span:nth-child(2) {
            animation-delay: 0.3s;
        }

        .wifi-progress-dots span:nth-child(3) {
            animation-delay: 0.6s;
        }

        @keyframes blink {
            0% {
                opacity: 0.2;
            }

            50% {
                opacity: 1;
            }

            100% {
                opacity: 0.2;
            }
        }

        /* ================= MOBILE MODE ================= */
        @media (max-width: 992px) {
            .menu-toggle {
                display: flex;
            }

            .sidebar {
                transform: translateX(-100%);
            }

            .sidebar.open {
                transform: translateX(0);
            }

            .sidebar-overlay.open {
                opacity: 1;
                visibility: visible;
            }
        }

        /* RESPONSIVE */
        @media (max-width: 992px) {
            .dashboard-container {
                grid-template-columns: 1fr;
            }

            /* TAMPILKAN TOMBOL ☰ DI HP */
            .menu-toggle {
                display: block;
            }

            /* SIDEBAR JADI SLIDE MENU */
            .sidebar {
                display: block;
                transform: translateX(-100%);
                transition: transform 0.3s ease;
            }

            .sidebar.open {
                transform: translateX(0);
            }

            .main-content {
                grid-column: 1;
                padding-top: 60px;
                /* supaya tidak ketutup tombol ☰ */
            }
        }
    </style>
</head>

<body>
    <!-- HAMBURGER BUTTON (MOBILE ONLY) -->
    <button id="menuToggle" class="menu-toggle">☰</button>
    <!-- Container Start -->
    <div class="dashboard-container">
        <!-- Sidebar navigation Star -->
        <div class="sidebar" id="mainSidebar">
            <div class="brand">
                <div class="brand-icon">🏠</div>
                <div class="brand-name">Home Monitoring</div>
            </div>
            <!-- Menu Star -->
            <ul class="nav-menu">
                <li class="nav-item">
                    <a href="#pf" class="nav-link active" onclick="showPage('pf')">
                        <span class="nav-icon">⚡</span>
                        <span>Energy</span>
                    </a>
                </li>
                <li class="nav-item">
                    <a href="#ultrasonic" class="nav-link" onclick="showPage('ultrasonic')">
                        <span class="nav-icon">🛢️</span>
                        <span>Ultrasonic</span>
                    </a>
                </li>

                <li class="nav-item">
                    <a href="#pressure" class="nav-link" onclick="showPage('pressure')">
                        <span class="nav-icon">🧭</span>
                        <span>Pressure</span>
                    </a>
                </li>
                <li class="nav-item">
                    <a href="#control" class="nav-link" onclick="showPage('control')">
                        <span class="nav-icon">🎮</span>
                        <span>Control</span>
                    </a>
                </li>
                <li class="nav-item">
                    <a href="#setting" class="nav-link" onclick="showPage('setting')">
                        <span class="nav-icon">⚙️</span>
                        <span>Setting</span>
                    </a>
                </li>

            </ul>
            <!-- Menu End -->
        </div>
        <!-- Sidebar Navigation -->

        <!-- Main Content Area Start -->
        <div class="main-content" id="mainContent">
            <!-- Energy Area Start -->
            <div id="pf-section" class="page-section active">
                <div class="page-header">
                    <div>
                        <h1 class="page-title">Energy Dashboard</h1>
                        <p class="page-subtitle">Real-time Monitoring of your prepaid energy system</p>
                    </div>
                    <div id="update-timestamp">Last updated: --:--:--</div>
                </div>

                <!-- Metric Cards Energy Sible Atas Start -->
                <div class="card-grid">
                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">VOLTAGE</h3>
                            <div class="card-icon">V</div>
                        </div>
                        <div class="dashboard-value" id="voltage">--<span class="unit">V</span></div>
                        <div class="trend">Line voltage</div>
                    </div>

                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">CURRENT</h3>
                            <div class="card-icon">A</div>
                        </div>
                        <div class="dashboard-value" id="current">--<span class="unit">A</span></div>
                        <div class="trend">Line current</div>
                    </div>

                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">POWER</h3>
                            <div class="card-icon">W</div>
                        </div>
                        <div class="dashboard-value" id="power">--<span class="unit">W</span></div>
                        <div class="trend">Active power</div>
                    </div>

                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">FREQUENCY</h3>
                            <div class="card-icon">Hz</div>
                        </div>
                        <div class="dashboard-value" id="frequency">--<span class="unit">Hz</span></div>
                        <div class="trend">Frequency</div>
                    </div>
                    <!-- Metric Cards Energy Sible Atas Start -->

                    <!-- Metric Cards Energy Sible Bawah Start -->
                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">_VOLTAGE</h3>
                            <div class="card-icon">V</div>
                        </div>
                        <div class="dashboard-value" id="_voltage">--<span class="unit">V</span></div>
                        <div class="trend">Line voltage</div>
                    </div>

                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">_CURRENT</h3>
                            <div class="card-icon">A</div>
                        </div>
                        <div class="dashboard-value" id="_current">--<span class="unit">A</span></div>
                        <div class="trend">Line current</div>
                    </div>

                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">_POWER</h3>
                            <div class="card-icon">W</div>
                        </div>
                        <div class="dashboard-value" id="_power">--<span class="unit">W</span></div>
                        <div class="trend">Active power</div>
                    </div>

                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">_FREQUENCY</h3>
                            <div class="card-icon">HZ</div>
                        </div>
                        <div class="dashboard-value" id="_frequency">--<span class="unit">Hz</span></div>
                        <div class="trend">Frequency</div>
                    </div>
                    <!-- Metric Cards Energy Sible Bawah End -->

                </div>
                <!-- Metric Cards Energy End -->
            </div>
            <!-- Energy Area End -->

            <!-- Ultrasonic Area Start -->
            <div id="ultrasonic-section" class="page-section">
                <div class="page-header">
                    <div>
                        <h1 class="page-title">Ultrasonic Dashboard</h1>
                        <p class="page-subtitle">Real-time Monitoring of your prepaid Ultrasonic system</p>
                    </div>
                    <div id="update-timestamp1">Last updated: --:--:--</div>
                </div>
                <!-- Metric Cards Ultrasonic Start -->
                <div class="card-grid">
                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">LEVEL AIR</h3>
                            <div class="card-icon">%</div>
                        </div>
                        <div class="dashboard-value" id="level">--<span class="unit">%</span></div>
                        <div class="trend">Line level</div>
                    </div>

                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">BATAS AIR</h3>
                            <div class="card-icon">cm</div>
                        </div>
                        <div class="dashboard-value" id="jarak">--<span class="unit">cm</span></div>
                        <div class="trend">Line cm</div>
                    </div>

                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">KETINGGIAN AIR</h3>
                            <div class="card-icon">cm</div>
                        </div>
                        <div class="dashboard-value" id="tinggi">--<span class="unit">cm</span></div>
                        <div class="trend">Line cm</div>
                    </div>
                    
                </div>
                <!-- Metric Cards Ultrasonic End -->
            </div>
            <!-- Ultrasonic Area End -->

            <!-- Pressure Area Start -->
            <div id="pressure-section" class="page-section">
                <div class="page-header">
                    <div>
                        <h1 class="page-title">Pressure Dashboard</h1>
                        <p class="page-subtitle">Real-time Monitoring of your prepaid Pressure system</p>
                    </div>
                </div>

                <!-- Metric Cards Pressure Start -->
                <div class="card-grid">
                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">PRESSURE / KG</h3>
                            <div class="card-icon">Kg</div>
                        </div>
                        <div class="dashboard-value" id="kg">--<span class="unit">Kg</span></div>
                        <div class="trend">Line Kg</div>
                    </div>

                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">PRESSURE / PSI</h3>
                            <div class="card-icon">Psi</div>
                        </div>
                        <div class="dashboard-value" id="psi">--<span class="unit">Psi</span></div>
                        <div class="trend">Line Psi</div>
                    </div>

                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">NILAI A</h3>
                            <div class="card-icon">Adc</div>
                        </div>
                        <div class="dashboard-value" id="kalmanPressureValue">--<span class="unit">Adc</span></div>
                        <div class="trend">Line Adc</div>
                    </div>
                </div>
                <!-- Metric Cards Pressure End -->
            </div>
            <!-- Pressure Area End -->

            <!-- Control Area Start -->
            <div id="control-section" class="page-section">
                <div class="page-header">
                    <div>
                        <h1 class="page-title">Control Dashboard</h1>
                        <p class="page-subtitle">Real-time Monitoring of your prepaid Pressure system</p>
                    </div>
                    <div id="update-timestamp2">Last updated: --:--:--</div>
                </div>

            </div>
            <!-- Control Area End -->

            <!-- ================= SETTING AREA START ================= -->
            <div id="setting-section" class="page-section">

                <div class="page-header">
                    <div>
                        <h1 class="page-title">Settings</h1>
                        <p class="page-subtitle">System configuration & maintenance</p>
                    </div>
                </div>
                <div class="card-grid">
                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">Firmware Update</h3>
                            <div class="firmware-version" id="version"></div>
                        </div>
                        <div class="action-content">
                            <p style="margin-bottom: 10px;">
                                Klik tombol di bawah ini untuk memeriksa dan menginstal firmware baru.
                            </p>
                            <button class="btn btn-primary btn-block" onclick="startOTA()">
                                UPDATE FIRMWARE
                            </button>
                            <div id="ota-status" class="alert" style="display:none; margin-top:10px;"></div>
                            <div id="ota-progress" style="display:none; margin-top:10px;">
                                <div style="width:100%; background:#e9ecef; border-radius:5px; overflow:hidden;">
                                    <div id="ota-progress-bar" class="progress-bar-animated" style="width:0%; height:20px;
                                            text-align:center; color:white; font-size:12px; line-height:20px;">
                                    </div>
                                </div>
                                <p id="ota-step" style="margin-top:5px; font-size:13px; color:#495057;">
                                    Idle
                                </p>
                            </div>
                            <hr style="margin:15px 0;">
                            <p>Reset WiFi untuk kembali ke mode Setup.</p>
                            <button id="btnResetWifi" class="btn btn-danger btn-block">RESET WIFI</button>
                            <p id="wifiResetStatus"></p>
                            <p id="wifiInfo" style="font-weight:600;"></p>

                        </div>
                    </div>
                    <!-- ============ WIFI CONFIG CARD (FINAL) ============ -->
                    <div class="card" id="wifiConfigCard">
                        <h3>WiFi Configuration</h3>
                        <p>Set WiFi agar ESP32 bisa online.</p>
                        <div class="form-group">
                            <label class="form-label">SSID (Scan WiFi)</label>
                            <div style="display:flex; gap:8px; align-items:center;">
                                <select id="wifiSSID" class="form-input" style="flex:1;">
                                    <option value="">-- Pilih WiFi --</option>
                                </select>
                                <button class="btn btn-primary" onclick="scanWiFi()" style="padding:10px 12px;">
                                    🔄 Scan
                                </button>
                            </div>
                        </div>
                        <div class="form-group">
                            <label class="form-label">WiFi Password</label>
                            <div style="display:flex; align-items:center;">
                                <input id="wifiPASS" class="form-input" type="password" placeholder="Password WiFi"
                                    style="flex:1; border-top-right-radius:0; border-bottom-right-radius:0;">
                                <button id="btnTogglePass" type="button" style="
                                            padding:10px 12px;
                                            border:1px solid #ced4da;
                                            background:#f0f2f5;
                                            cursor:pointer;
                                            border-top-right-radius:5px;
                                            border-bottom-right-radius:5px;
                                            font-size:16px;">👁️
                                </button>
                            </div>
                        </div>
                        <button class="btn btn-success" onclick="saveWiFi()">SIMPAN WIFI</button>
                        <div id="wifiConnectingBox" style="display:none;">
                            <div class="loader"></div>
                            <p id="wifiConnectingText"></p>
                        </div>
                        <p id="wifiResult" style="margin-top:10px; font-weight:600;"></p>
                        <p id="wifiResetStatus"></p>
                        <p id="wifiInfo"></p>
                    </div>
                </div>
            </div>
        </div>
        <!-- ================= SETTING AREA END ================= -->
        <div class="dashboard-footer">
            <p id="update-status">Last Update: --</p>
            <p>Prepaid Home Monitor Pro © 2026</p>
        </div>

    </div>
    <!-- Main Content Area End -->
    </div>
    <!-- Container end -->
    <script>
        let relayState = false; // Track relay state on client-side

        // Show selected page section
        function showPage(page) {
            // Hide all page sections
            const pageSections = document.querySelectorAll('.page-section');
            pageSections.forEach(section => {
                section.classList.remove('active');
            });

            // Show selected page section
            document.getElementById(page + '-section').classList.add('active');

            // Update active nav link
            const navLinks = document.querySelectorAll('.nav-link');
            navLinks.forEach(link => {
                link.classList.remove('active');
            });
            document.querySelector(`.nav-link[href="#${page}"]`).classList.add('active');

        }

        function updateValues() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    // Dashboard Section Updates
                    updateDashboardEnergyValues(data);

                    // Account Section Updates
                    ubdatedashboardUltrasonicValues(data);

                    // Control Section Updates
                    ubdatedashboardPressureValues(data);

                    // Update timestamp
                    const now = new Date();
                    document.getElementById('update-timestamp').textContent = `Last updated: ${now.toLocaleTimeString()}`;
                    document.getElementById('update-timestamp1').textContent = `Last updated: ${now.toLocaleTimeString()}`;
                    document.getElementById('update-timestamp2').textContent = `Last updated: ${now.toLocaleTimeString()}`;
                    document.getElementById('update-status').textContent = `Last Update: ${now.toLocaleTimeString()}`;
                    document.getElementById('version').textContent = `${data.version}`;

                });
        }

        function updateDashboardEnergyValues(data) {
            updateElement('voltage', data.voltage.toFixed(1) + '<span class="unit">V</span>');
            updateElement('current', data.current.toFixed(3) + '<span class="unit">A</span>');
            updateElement('power', data.power.toFixed(1) + '<span class="unit">W</span>');
            updateElement('frequency', data.frequency.toFixed(1) + '<span class="unit">Hz</span>');

            updateElement('_voltage', data._voltage.toFixed(1) + '<span class="unit">V</span>');
            updateElement('_current', data._current.toFixed(3) + '<span class="unit">A</span>');
            updateElement('_power', data._power.toFixed(1) + '<span class="unit">W</span>');
            updateElement('_frequency', data._frequency.toFixed(1) + '<span class="unit">Hz</span>');

        }

        function ubdatedashboardUltrasonicValues(data) {
            updateElement('jarak', data.jarak.toFixed(1) + '<span class="unit">Cm</span>');
            updateElement('level', data.level.toFixed(1) + '<span class="unit">%</span>');
            updateElement('tinggi', data.tinggi.toFixed(1) + '<span class="unit">Cm</span>');

        }

        function ubdatedashboardPressureValues(data) {
            updateElement('kg', data.kg.toFixed(1) + '<span class="unit">Kg</span>');
            updateElement('psi', data.psi.toFixed(1) + '<span class="unit">Psi</span>');
            updateElement('kalmanPressureValue', data.kalmanPressureValue.toFixed(1) + '<span class="unit">Adc</span>');
            // updateElement('version', data.version);
        }

        function updateElement(id, html) {
            const element = document.getElementById(id);
            if (element) {
                element.innerHTML = html;
            }
        }

        function showAlert(message, type, alertElementId) {
            const alertDiv = document.getElementById(alertElementId);
            if (alertDiv) {
                alertDiv.textContent = message;
                alertDiv.className = `alert alert-${type}`;
                alertDiv.style.display = 'block';
                setTimeout(() => {
                    alertDiv.style.display = 'none';
                }, 5000);
            }
        }

        function showWifiModal(title, text) {
            document.getElementById("wifiModalTitle").innerText = title;
            document.getElementById("wifiModalText").innerText = text;
            document.getElementById("wifiModal").style.display = "flex";
        }

        function hideWifiModal() {
            document.getElementById("wifiModal").style.display = "none";
        }

        function startOTA() {

            if (!confirm("Melakukan Ubdate firmware bisa jadi tidak sesuai dengan sebulumnya, tekan oke jika mau melanjutkan,tekan cancel jika mau membatalkan ")) {
                return;
            }

            const alertBox = document.getElementById('ota-status');
            const progressBox = document.getElementById('ota-progress');
            const stepText = document.getElementById('ota-step');

            alertBox.style.display = "block";
            alertBox.className = "alert alert-warning";
            alertBox.textContent = "Checking for update...";
            progressBox.style.display = "block";
            updateProgress(0, "Checking...");

            fetch('/ota')
                .then(r => r.text())
                .then(msg => {
                    if (msg === "UP_TO_DATE") {
                        alert("✅ Firmware already up to date!");
                        return;
                    }
                    pollOtaProgress();
                })
                .catch(err => {
                    alertBox.className = "alert alert-danger";
                    alertBox.textContent = "OTA request failed!";
                });
        }

        let displayedProgress = 0;
        let progressInterval = null; // <- penting biar tidak numpuk interval

        function updateProgress(targetPercent, step) {
            const bar = document.getElementById("ota-progress-bar");
            const stepText = document.getElementById("ota-step");

            stepText.textContent = step;

            // HENTIKAN interval lama dulu (biar tidak tabrakan)
            if (progressInterval) {
                clearInterval(progressInterval);
            }

            progressInterval = setInterval(() => {
                if (displayedProgress < targetPercent) {
                    displayedProgress += 1;
                }
                else if (displayedProgress > targetPercent) {
                    displayedProgress -= 1;
                }
                else {
                    clearInterval(progressInterval);
                }

                bar.style.width = displayedProgress + "%";
                bar.textContent = displayedProgress + "%";

                // Jika sudah 100%, hentikan animasi gradient
                if (displayedProgress >= 100) {
                    bar.style.animation = "none";
                }

            }, 20);
        }

        function pollOtaProgress() {
            const alertBox = document.getElementById('ota-status');

            const interval = setInterval(() => {
                fetch('/otaProgress')
                    .then(r => r.text())
                    .then(data => {
                        const [percent, status] = data.split("|");

                        const p = parseInt(percent);
                        if (!isNaN(p)) {
                            updateProgress(p, status);
                        }

                        if (status === "DOWNLOADING") {
                            alertBox.className = "alert alert-warning";
                            alertBox.textContent = "Downloading firmware...";
                        }
                        else if (status === "FLASHING") {
                            alertBox.className = "alert alert-warning";
                            alertBox.textContent = "Installing firmware, please wait...";
                        }
                        else if (status === "REBOOT") {
                            alertBox.className = "alert alert-success";
                            alertBox.textContent = "Rebooting device...";
                            updateProgress(100, "REBOOT"); // <-- PASTIKAN 100%
                            clearInterval(interval);
                        }
                    })
                    .catch(err => {
                        alertBox.className = "alert alert-danger";
                        alertBox.textContent = "Lost connection to device.";
                        clearInterval(interval);
                    });
            }, 1000);
        }

        // ======= MOBILE MENU TOGGLE =======
        const menuToggle = document.getElementById("menuToggle");
        const sidebar = document.querySelector(".sidebar");

        // Buka / tutup sidebar saat klik ☰
        menuToggle.addEventListener("click", () => {
            sidebar.classList.toggle("open");
        });

        // Tutup sidebar otomatis saat klik menu (di HP)
        document.querySelectorAll(".nav-link").forEach(link => {
            link.addEventListener("click", () => {
                if (window.innerWidth <= 992) {
                    sidebar.classList.remove("open");
                }
            });
        });

        function saveWiFi() {
            const ssid = document.getElementById("wifiSSID").value;
            const pass = document.getElementById("wifiPASS").value;

            if (!ssid) {
                alert("SSID tidak boleh kosong!");
                return;
            }

            // Tampilkan loader
            document.getElementById("wifiConnectingBox").style.display = "block";
            document.getElementById("wifiConnectingText").innerText =
                "Mencoba menghubungkan ke WiFi: " + ssid + " ...";

            const resultBox = document.getElementById("wifiResult");
            resultBox.innerText = "";
            resultBox.className = "";

            fetch('/wifi/save', {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({
                    ssid: ssid,
                    password: pass
                })
            })
                .then(r => r.text())
                .then(msg => {

                    // Sembunyikan loader
                    document.getElementById("wifiConnectingBox").style.display = "none";

                    if (msg === "OK_CONNECTED") {
                        resultBox.innerText = "✅ WiFi berhasil terhubung! ESP32 akan restart...";
                        resultBox.className = "success";
                    }
                    else if (msg === "FAIL_CONNECT") {   // 🔹 DIBETULKAN
                        resultBox.innerText = "❌ Gagal konek! Periksa SSID / Password.";
                        resultBox.className = "fail";
                    }
                    else {
                        resultBox.innerText = "⚠️ Respons tidak dikenal: " + msg;
                        resultBox.className = "fail";
                    }
                })
                .catch(err => {
                    document.getElementById("wifiConnectingBox").style.display = "none";
                    resultBox.innerText =
                        "❌ Tidak dapat mengirim data ke ESP32! Pastikan Anda terhubung ke AP ESP32.";
                    resultBox.className = "fail";
                });
        }

        document.getElementById("btnResetWifi").onclick = async function () {
            if (!confirm("Yakin reset WiFi? Semua password akan terhapus.")) return;

            document.getElementById("wifiResetStatus").innerText = "Resetting WiFi...";

            await fetch("/wifi/reset");

            document.getElementById("wifiResetStatus").innerText =
                "WiFi direset! Sambungkan ke: ESP32-Panel-Setup (192.168.4.1)";

            // Update tampilan info WiFi (biar bersih)
            document.getElementById("wifiInfo").innerText = "Belum terhubung ke WiFi";
        };

        // Fungsi tunggal untuk baca semua info WiFi (lebih rapi)
        async function updateWiFiInfo() {
            try {
                const r = await fetch('/wifi/info');
                const info = await r.json();

                document.getElementById("wifiResetStatus").innerText =
                    "ESP32 Client IP: " + info.ip;

                document.getElementById("wifiInfo").innerHTML =
                    `📶 SSID: <b>${info.ssid}</b> | ` +
                    `IP: <b>${info.ip}</b> | ` +
                    `RSSI: <b>${info.rssi} dBm</b>`;
            } catch (e) {
                document.getElementById("wifiResetStatus").innerText =
                    "Tidak dapat membaca IP ESP32";
                document.getElementById("wifiInfo").innerText =
                    "Tidak dapat membaca info WiFi";
            }
        }

        async function scanWiFi() {
            const ssidSelect = document.getElementById("wifiSSID");

            // Tampilkan loading sementara
            ssidSelect.innerHTML = "<option>🔍 Scanning...</option>";

            try {
                const res = await fetch("/wifi/scan");
                const networks = await res.json();

                ssidSelect.innerHTML = "<option value=''>-- Pilih WiFi --</option>";

                networks.forEach(net => {
                    const opt = document.createElement("option");
                    opt.value = net.ssid;
                    opt.textContent = `${net.ssid} (${net.rssi} dBm)`;
                    ssidSelect.appendChild(opt);
                });

            } catch (e) {
                ssidSelect.innerHTML = "<option>❌ Gagal scan</option>";
            }
        }

        // ======= TOGGLE SHOW / HIDE PASSWORD =======
        document.getElementById("btnTogglePass").addEventListener("click", function () {
            const passInput = document.getElementById("wifiPASS");

            if (passInput.type === "password") {
                passInput.type = "text";
                this.innerText = "🙈";   // ikon berubah saat terlihat
            } else {
                passInput.type = "password";
                this.innerText = "👁️";
            }
        });

        // Jalankan sekali saat halaman dimuat
        updateWiFiInfo();

        // Auto scan saat halaman Settings pertama kali dibuka
        scanWiFi();

        setInterval(updateWiFiInfo, 5000);
        setInterval(updateValues, 2000);

        // Set initial page to dashboard
        showPage('pf');
        // ================== WIFI INFO (IP + SSID + RSSI) ==================

    </script>

    <!-- ================= FULLSCREEN WIFI LOADING MODAL ================= -->
    <div id="wifiModal" class="wifi-modal">
        <div class="wifi-modal-content">
            <div class="wifi-spinner"></div>
            <h3 id="wifiModalTitle">Menghubungkan ke WiFi...</h3>
            <p id="wifiModalText">Mohon tunggu, ESP32 sedang menguji koneksi.</p>

            <div class="wifi-progress-dots">
                <span>.</span><span>.</span><span>.</span>
            </div>
        </div>
    </div>

</body>

</html>
)===";