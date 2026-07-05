SELECT
  date(occurred_at) AS event_date,
  app_version,
  os_name,
  SUM(CASE WHEN event_name = 'app_opened' THEN 1 ELSE 0 END) AS app_opened,
  SUM(CASE WHEN event_name = 'scan_started' THEN 1 ELSE 0 END) AS scan_started,
  SUM(CASE WHEN event_name = 'scan_completed' THEN 1 ELSE 0 END) AS scan_completed,
  SUM(CASE WHEN event_name = 'repair_started' THEN 1 ELSE 0 END) AS repair_started,
  SUM(CASE WHEN event_name = 'repair_completed' THEN 1 ELSE 0 END) AS repair_completed,
  SUM(CASE WHEN event_name = 'verification_completed' THEN 1 ELSE 0 END) AS verification_completed,
  SUM(CASE WHEN event_name = 'ready_reached' THEN 1 ELSE 0 END) AS ready_reached,
  COUNT(DISTINCT installation_id) AS unique_installations
FROM events
WHERE occurred_at >= datetime('now', '-' || COALESCE(:since_days, 30) || ' days')
GROUP BY event_date, app_version, os_name
ORDER BY event_date DESC, app_version, os_name;
