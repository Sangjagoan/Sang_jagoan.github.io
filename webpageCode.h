
// HTML page stored in PROGMEM (Improved Web Dashboard UI/UX) - Modified to include Factory Reset Button
const char webpageCode[] PROGMEM = R"===(
`
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
            box-sizing: border-box;
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
            min-height: 100vh;
        }

        /* Sidebar */
        .sidebar {
            background-color: var(--primary);
            color: white;
            padding-top: 20px;
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

        /* Hide sections */
        .page-section {
            display: none;
        }

        .page-section.active {
            display: block;
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

        /* Responsive */
        @media (max-width: 992px) {
            .dashboard-container {
                grid-template-columns: 1fr;
            }

            .sidebar {
                display: none;
            }

            .main-content {
                grid-column: 1;
            }
        }
    </style>
</head>

<body>
    <!-- Container Start -->
    <div class="dashboard-container">
        <!-- Sidebar navigation Star -->
        <div class="sidebar">
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
            </ul>
            <!-- Menu End -->
        </div>
        <!-- Sidebar Navigation -->

        <!-- Main Content Area Start -->
        <div class="main-content">
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
                            <div class="card-icon">Cm</div>
                        </div>
                        <div class="dashboard-value" id="jarak">--<span class="unit">Cm</span></div>
                        <div class="trend">Line cm</div>
                    </div>

                    <div class="card">
                        <div class="card-header">
                            <h3 class="card-title">KETINGGIAN AIR</h3>
                            <div class="card-icon">Cm</div>
                        </div>
                        <div class="dashboard-value" id="tinggi">--<span class="unit">Cm</span></div>
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
                    <div id="update-timestamp2">Last updated: --:--:--</div>
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
        // Initial update and set interval
        updateValues();
        setInterval(updateValues, 3000); // Update every 3 seconds

        // Set initial page to dashboard
        showPage('pf');
    </script>

</body>

</html>
)===";
