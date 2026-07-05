SELECT
  date(occurred_at) AS event_date,
  app_version,
  os_name,
  error_code,
  COUNT(*) AS event_count,
  COUNT(DISTINCT installation_id) AS affected_installations
FROM events
WHERE
  error_code IS NOT NULL
  AND error_code <> ''
  AND occurred_at >= datetime('now', '-' || COALESCE(:since_days, 30) || ' days')
GROUP BY event_date, app_version, os_name, error_code
ORDER BY event_date DESC, event_count DESC, app_version, os_name, error_code;
