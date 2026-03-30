export function formatLongDate(iso) {
  return new Date(iso).toLocaleString("en-US", {
    month: "short",
    day: "numeric",
    year: "numeric",
    hour: "numeric",
    minute: "2-digit",
  });
}

export function withinDateFilter(iso, filter) {
  if (filter === "Any time") {
    return true;
  }

  const now = new Date("2026-03-17T18:00:00");
  const date = new Date(iso);
  const diff = now.getTime() - date.getTime();
  const oneDay = 24 * 60 * 60 * 1000;

  if (filter === "Today") {
    return (
      date.getFullYear() === now.getFullYear() &&
      date.getMonth() === now.getMonth() &&
      date.getDate() === now.getDate()
    );
  }

  if (filter === "Last 7 days") {
    return diff >= 0 && diff <= 7 * oneDay;
  }

  if (filter === "Last 30 days") {
    return diff >= 0 && diff <= 30 * oneDay;
  }

  return true;
}

export function runMailUtilsTests() {
  const tests = [
    withinDateFilter("2026-03-17T10:00:00", "Today") === true,
    withinDateFilter("2026-03-14T10:00:00", "Last 7 days") === true,
    withinDateFilter("2026-02-01T10:00:00", "Last 7 days") === false,
    typeof formatLongDate("2026-03-17T10:45:00") === "string",
  ];

  return tests.every(Boolean);
}