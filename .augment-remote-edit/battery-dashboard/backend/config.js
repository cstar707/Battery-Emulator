const fs = require('fs');

const LEGACY_TESLA_TOPICS = Object.freeze({
  info: 'BE/info',
  spec: 'BE/spec_data',
  balancing: 'BE/balancing_data',
});

const LEGACY_ENVOY_POWER_TOPICS = Object.freeze({
  envoy1: 'envoy/1/active_power',
  envoy2: 'envoy/2/active_power',
});

const SHARED_SETTINGS_FILE = process.env.INTEGRATION_SETTINGS_FILE || '/home/chad/solar-monitoring/solis_s6_app/settings.json';

function loadSharedSettings() {
  try {
    if (!fs.existsSync(SHARED_SETTINGS_FILE)) return {};
    return JSON.parse(fs.readFileSync(SHARED_SETTINGS_FILE, 'utf8'));
  } catch (err) {
    console.warn('[config] failed to read shared integration settings:', err.message);
    return {};
  }
}

function toInt(value, fallback) {
  const parsed = parseInt(value, 10);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function getSetting(key, envKey, fallback, transform = (value) => value) {
  const envValue = process.env[envKey];
  if (envValue != null && String(envValue).trim() !== '') return transform(envValue);
  const settings = loadSharedSettings();
  const storedValue = settings[key];
  if (storedValue != null && String(storedValue).trim() !== '') return transform(storedValue);
  return fallback;
}

function getBoolSetting(key, envKey, fallback) {
  const envValue = process.env[envKey];
  if (envValue != null && String(envValue).trim() !== '') {
    return ['1', 'true', 'yes', 'on'].includes(String(envValue).trim().toLowerCase());
  }
  const settings = loadSharedSettings();
  const storedValue = settings[key];
  if (storedValue == null) return fallback;
  if (typeof storedValue === 'boolean') return storedValue;
  return ['1', 'true', 'yes', 'on'].includes(String(storedValue).trim().toLowerCase());
}

function normalizePath(value) {
  const raw = String(value || '').trim();
  if (!raw) return '/';
  return raw.startsWith('/') ? raw : `/${raw}`;
}

function buildMqttUrl() {
  if (process.env.MQTT_URL) return process.env.MQTT_URL;
  const host = String(getSetting('mqtt_host', 'MQTT_HOST', 'localhost', (value) => String(value).trim()) || 'localhost');
  const port = getSetting('mqtt_port', 'MQTT_PORT', 1883, (value) => toInt(value, 1883));
  return `mqtt://${host}:${port}`;
}

function buildBatterySource(prefix, defaults) {
  const upper = prefix.toUpperCase();
  return {
    enabled: getBoolSetting(`${prefix}_source_enabled`, `${upper}_CELL_SOURCE_ENABLED`, defaults.enabled),
    type: String(getSetting(`${prefix}_source_type`, `${upper}_CELL_SOURCE_TYPE`, defaults.type, (value) => String(value).trim().toLowerCase()) || defaults.type),
    specTopic: String(getSetting(`${prefix}_source_spec_topic`, `${upper}_CELL_SOURCE_SPEC_TOPIC`, defaults.specTopic, (value) => String(value).trim()) || ''),
    balancingTopic: String(getSetting(`${prefix}_source_balancing_topic`, `${upper}_CELL_SOURCE_BALANCING_TOPIC`, defaults.balancingTopic, (value) => String(value).trim()) || ''),
    infoTopic: String(getSetting(`${prefix}_source_info_topic`, `${upper}_CELL_SOURCE_INFO_TOPIC`, defaults.infoTopic, (value) => String(value).trim()) || ''),
    host: String(getSetting(`${prefix}_source_host`, `${upper}_CELL_SOURCE_HOST`, defaults.host, (value) => String(value).trim()) || ''),
    port: getSetting(`${prefix}_source_port`, `${upper}_CELL_SOURCE_PORT`, defaults.port, (value) => toInt(value, defaults.port)),
    basePath: normalizePath(getSetting(`${prefix}_source_base_path`, `${upper}_CELL_SOURCE_BASE_PATH`, defaults.basePath, (value) => String(value).trim())),
  };
}

const config = {
  server: {
    port: parseInt(process.env.PORT || '3008', 10),
  },
  get sharedSettingsFile() {
    return SHARED_SETTINGS_FILE;
  },
  get mqtt() {
    return {
      url: buildMqttUrl(),
      username: process.env.MQTT_USERNAME || '',
      password: process.env.MQTT_PASSWORD || '',
      topicSolark: process.env.MQTT_TOPIC_SOLARK || 'solar/solark/sensors/#',
      topicSolarkJson: String(getSetting('solark_mqtt_topic', 'MQTT_TOPIC_SOLARK_JSON', 'solar/solark', (value) => String(value).trim()) || ''),
      topicCells: process.env.CELL_MQTT_TOPIC || 'BE/spec_data',
      topicTeslaBe: String(getSetting('tesla_source_info_topic', 'MQTT_TOPIC_TESLA_BE', 'BE/info', (value) => String(value).trim()) || ''),
      topicBeSpec: String(getSetting('tesla_source_spec_topic', 'MQTT_TOPIC_BE_SPEC', 'BE/spec_data', (value) => String(value).trim()) || ''),
      topicBeBalancing: String(getSetting('tesla_source_balancing_topic', 'MQTT_TOPIC_BE_BALANCING', 'BE/balancing_data', (value) => String(value).trim()) || ''),
      compatTeslaInfoTopic: process.env.MQTT_TOPIC_TESLA_BE_LEGACY || LEGACY_TESLA_TOPICS.info,
      compatTeslaSpecTopic: process.env.MQTT_TOPIC_BE_SPEC_LEGACY || LEGACY_TESLA_TOPICS.spec,
      compatTeslaBalancingTopic: process.env.MQTT_TOPIC_BE_BALANCING_LEGACY || LEGACY_TESLA_TOPICS.balancing,
      topicEnvoy1: process.env.MQTT_TOPIC_ENVOY1 || 'solar/envoy1',
      topicEnvoy2: process.env.MQTT_TOPIC_ENVOY2 || 'solar/envoy2',
      compatTopicEnvoy1Power: process.env.MQTT_TOPIC_ENVOY1_ACTIVE_POWER || LEGACY_ENVOY_POWER_TOPICS.envoy1,
      compatTopicEnvoy2Power: process.env.MQTT_TOPIC_ENVOY2_ACTIVE_POWER || LEGACY_ENVOY_POWER_TOPICS.envoy2,
    };
  },
  solis: {
    apiUrl: process.env.SOLIS_API_URL || 'http://localhost:3007',
  },
  envoy: {
    apiUrl: process.env.ENVOY_API_URL || process.env.SOLAR_API_URL || 'http://localhost:3004',
  },
  influxdb: {
    url: process.env.INFLUXDB_URL || 'http://localhost:8086',
    token: process.env.INFLUXDB_TOKEN || '',
    org: process.env.INFLUXDB_ORG || 'solar',
    bucket: process.env.INFLUXDB_BUCKET || 'solar_monitoring',
  },
  get battery() {
    return {
      cellCount: parseInt(process.env.CELL_COUNT || '96', 10),
      voltageUnit: process.env.CELL_VOLTAGE_UNIT || 'mV',
      defaultSourceBattery: String(getSetting('default_battery_source', 'DEFAULT_BATTERY_SOURCE', 'tesla', (value) => String(value).trim().toLowerCase()) || 'tesla'),
    };
  },
  get batterySources() {
    return {
      tesla: buildBatterySource('tesla', {
        enabled: true,
        type: 'mqtt',
        specTopic: 'BE/spec_data',
        balancingTopic: 'BE/balancing_data',
        infoTopic: 'BE/info',
        host: '',
        port: 80,
        basePath: '/',
      }),
      ruxiu: buildBatterySource('ruxiu', {
        enabled: true,
        type: 'mock',
        specTopic: '',
        balancingTopic: '',
        infoTopic: '',
        host: '',
        port: 80,
        basePath: '/',
      }),
    };
  },
  app: {
    liveRefreshSeconds: parseInt(process.env.LIVE_REFRESH_SECONDS || '5', 10),
    historyHours: parseInt(process.env.HISTORY_HOURS || '24', 10),
  },
};

module.exports = config;
