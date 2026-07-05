WITH opened AS (
  SELECT
    date(occurred_at) AS event_date,
    app_version,
    os_name,
    COUNT(*) AS app_opened,
    COUNT(DISTINCT installation_id) AS opened_installations
  FROM events
  WHERE
    event_name = 'app_opened'
    AND occurred_at >= datetime('now', '-' || COALESCE(:since_days, 30) || ' days')
  GROUP BY event_date, app_version, os_name
),
ready AS (
  SELECT
    date(occurred_at) AS event_date,
    app_version,
    os_name,
    COUNT(*) AS ready_reached,
    COUNT(DISTINCT installation_id) AS ready_installations
  FROM events
  WHERE
    event_name = 'ready_reached'
    AND occurred_at >= datetime('now', '-' || COALESCE(:since_days, 30) || ' days')
  GROUP BY event_date, app_version, os_name
)
SELECT
  opened.event_date,
  opened.app_version,
  opened.os_name,
  opened.app_opened,
  COALESCE(ready.ready_reached, 0) AS ready_reached,
  CASE
    WHEN opened.app_opened = 0 THEN 0
    ELSE CAST(COALESCE(ready.ready_reached, 0) AS REAL) / opened.app_opened
  END AS ready_reached_per_app_opened,
  opened.opened_installations,
  COALESCE(ready.ready_installations, 0) AS ready_installations,
  CASE
    WHEN opened.opened_installations = 0 THEN 0
    ELSE CAST(COALESCE(ready.ready_installations, 0) AS REAL) / opened.opened_installations
  END AS ready_installation_conversion
FROM opened
LEFT JOIN ready
  ON ready.event_date = opened.event_date
  AND ready.app_version = opened.app_version
  AND ready.os_name = opened.os_name
ORDER BY opened.event_date DESC, opened.app_version, opened.os_name;
