// Code.gs - Google Apps Script Web App Absensi RFID Toggle
// Deploy: Publish > Deploy as Web App (Anyone with the link)
// Endpoint contoh:
//   Cek status terakhir:    https://.../exec?mode=check&nama=John
//   Absen (toggle):         https://.../exec?mode=attendance&nama=John
//   Paksa status:           https://.../exec?mode=attendance&nama=John&status=MASUK
//
// Response JSON contoh:
//   { "ok": true, "nama": "John", "lastStatus": "MASUK" }
//   { "ok": true, "nama": "John", "status": "KELUAR", "previous": "MASUK", "date": "11/11/2025", "time": "19:55:20" }

var ss = SpreadsheetApp.openById('1x5_pUXKRsm-rjs53l2IOwa4mmQ-DhqfIb7cNXvbSdC4');
var sheet = ss.getSheetByName('Sheet2');
var TIMEZONE = "Asia/Jakarta";

function ensureHeader() {
  if (!sheet) sheet = ss.insertSheet('Sheet2');
  if (sheet.getLastRow() === 0) {
    sheet.appendRow(['Nama', 'Tanggal', 'Waktu', 'Status']);
  }
}

function doGet(e) {
  return handleRequest(e);
}

function doPost(e) {
  return handleRequest(e);
}

function handleRequest(e) {
  ensureHeader();
  var params = extractParams(e);

  var namaRaw = stripQuotes(params.nama || params.data || "");
  var mode = (params.mode || params.action || "").toString().trim().toLowerCase();
  var statusInput = (params.status || "").toString().trim().toUpperCase();

  if (!namaRaw) {
    return jsonOut({ ok: false, error: "Nama kosong" });
  }
  var nama = namaRaw.trim();

  if (mode === "check") {
    var last = getLastStatusByName(nama);
    return jsonOut({ ok: true, nama: nama, lastStatus: last || "" });
  }

  if (mode === "attendance" || mode === "absen" || mode === "presence") {
    var last = getLastStatusByName(nama);
    var now = new Date();
    var dateStr = Utilities.formatDate(now, TIMEZONE, "dd/MM/yyyy");
    var timeStr = Utilities.formatDate(now, TIMEZONE, "HH:mm:ss");

    var newStatus;
    if (statusInput === "MASUK" || statusInput === "KELUAR") {
      newStatus = statusInput; // override manual
    } else {
      newStatus = (last === "MASUK") ? "KELUAR" : "MASUK"; // toggle otomatis
    }

    sheet.appendRow([nama, dateStr, timeStr, newStatus]);

    return jsonOut({
      ok: true,
      nama: nama,
      status: newStatus,
      previous: last || "",
      date: dateStr,
      time: timeStr
    });
  }

  return jsonOut({ ok: false, error: "Unknown mode. Gunakan mode=check atau mode=attendance" });
}

function extractParams(e) {
  var p = {};
  // JSON body
  if (e && e.postData && e.postData.type && e.postData.type.indexOf("application/json") === 0) {
    try { p = JSON.parse(e.postData.contents || "{}"); } catch (err) {}
  }
  // URL params
  if (e && e.parameter) {
    for (var k in e.parameter) p[k] = e.parameter[k];
  }
  return p;
}

function getLastStatusByName(nama) {
  var data = sheet.getDataRange().getValues();
  for (var i = data.length - 1; i >= 1; i--) {
    if (String(data[i][0]).trim().toLowerCase() === nama.toLowerCase()) {
      return String(data[i][3]).trim().toUpperCase();
    }
  }
  return "";
}

function stripQuotes(v) {
  return v ? v.toString().replace(/^["']|['"]$/g, "") : "";
}

function jsonOut(obj) {
  var out = ContentService.createTextOutput(JSON.stringify(obj));
  out.setMimeType(ContentService.MimeType.JSON);
  return out;
}
