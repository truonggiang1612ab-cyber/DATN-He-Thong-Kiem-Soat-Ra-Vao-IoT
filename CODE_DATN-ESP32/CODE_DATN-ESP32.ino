#include "Important_information.h"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <LittleFS.h>

// Cấu hình phần cứng và kết nối
#define STM_RX 16
#define STM_TX 17
#define TLS_INSECURE_MODE 1
HardwareSerial STM32(2);
WiFiClientSecure firebaseClient;
WiFiClientSecure telegramClient;

// Cấu hình thời gian
const long gmtOffset_sec = 7L * 3600L;
const int daylightOffset_sec = 0;
const unsigned long NTP_RETRY_INTERVAL = 1000UL;
const unsigned long TIME_SEND_INTERVAL = 1000UL;
bool ntpStarted = false;
bool ntpSynchronized = false;
unsigned long ntpRetryTick = 0;
unsigned long timeSendTick = 0;
String lastBlynkTime = "";
String lastBlynkDate = "";

// Giao tiếp với STM32
String uart_line = "";

// Kết nối wifi và blynk
const unsigned long WIFI_RETRY_INTERVAL = 5000UL;
const unsigned long BLYNK_RETRY_INTERVAL = 3000UL;
unsigned long wifiTick = 0;
unsigned long blynkTick = 0;
bool wifiWasConnected = false;
bool blynkWasConnected = false;

// Đồng bộ nội dung LCD với LCD blynk
String lastLcd1FromSTM32 = "";
String lastLcd2FromSTM32 = "";
String lastBlynkLcd1 = "";
String lastBlynkLcd2 = "";
String log_text = "";
bool demoSyncEnabled = false;

// Dữ liệu lần truy cập hiện tại
String last_user = "";
String last_action = "";
String last_floor = "";
String last_room = "";
String last_time = "";
String last_date = "";

// Cảnh báo khẩn cấp
const unsigned long EMERGENCY_EVENT_TIMEOUT = 10000UL;
const unsigned long TELEGRAM_RETRY_INTERVAL = 10000UL;
bool emergency_event_pending = false;
bool emergency_request = false;
unsigned long emergency_event_tick = 0;
unsigned long telegramRetryTick = 0;
String emergency_user = "";
String emergency_action = "";
String emergency_floor = "";
String emergency_room = "";
String emergency_time = "";
String emergency_date = "";

// Xác thực Firebase
String firebaseIdToken = "";
String firebaseRefreshToken = "";
unsigned long firebaseTokenTick = 0;
unsigned long firebaseTokenLifetimeMs = 3300000UL;

// Lệnh quản lý người dùng Firebase với STM32
const unsigned long USER_COMMAND_POLL_INTERVAL = 2000UL;
const unsigned long USER_COMMAND_TIMEOUT = 60000UL;
const unsigned long USER_DATABASE_SYNC_INTERVAL = 3000UL;
unsigned long userCommandPollTick = 0;
unsigned long activeCommandTick = 0;
unsigned long activeCommandSyncTick = 0;
String activeCommandId = "";
String activeCommandType = "";
String activeCommandUserId = "";
String activeCommandName = "";
String activeCommandVersion = "";
String activeCommandFloor = "";
String activeCommandRoom = "";
String activeCommandActivationCode = "";
String activeCommandDoneResult = "OK";
String activeCommandFailureResult = "";
bool activeCommandHardwareDone = false;
bool activeCommandRecovery = false;
bool activeCommandFailed = false;

// Hàng đợi sự kiện kích hoạt người dùng
struct UserEventData
{
  String userId;
  String status;
  String version;
};
const uint8_t USER_EVENT_QUEUE_SIZE = 16;
const unsigned long USER_EVENT_SYNC_INTERVAL = 3000UL;
UserEventData userEventQueue[USER_EVENT_QUEUE_SIZE];
uint8_t userEventHead = 0;
uint8_t userEventTail = 0;
uint8_t userEventCount = 0;
unsigned long userEventSyncTick = 0;

// Hàng đợi nhật ký offline
const char OFFLINE_LOG_FILE[] = "/offline_logs.txt";
const char OFFLINE_TEMP_FILE[] = "/offline_logs.tmp";
const unsigned long OFFLINE_SYNC_INTERVAL = 3000UL;
const uint8_t OFFLINE_SYNC_BATCH_SIZE = 3;
unsigned long offlineSyncTick = 0;  

// Thoát các ký tự đặc biệt để chuỗi có thể chèn an toàn vào JSON
String jsonEscape(const String &s)
{
  const char hex[] = "0123456789ABCDEF";
  String output;
  output.reserve(s.length() + 16);
  for(size_t i = 0; i < s.length(); i++)
  {
    uint8_t c = (uint8_t)s[i];
    switch(c)
    {
      case '\"': output += "\\\""; break;
      case '\\': output += "\\\\"; break;
      case '\b': output += "\\b";  break;
      case '\f': output += "\\f";  break;
      case '\n': output += "\\n";  break;
      case '\r': output += "\\r";  break;
      case '\t': output += "\\t";  break;
      default:
      {
        if(c < 0x20)
        {
          output += "\\u00";
          output += hex[(c >> 4) & 0x0F];
          output += hex[c & 0x0F];
        }
        else
        {
          output += (char)c;
        }
        break;
      }
    }
  }
  return output;
}
// Lấy giá trị của một trường trong chuỗi JSON đơn giản
// Hỗ trợ cả giá trị dạng chuỗi và dạng số, trạng thái hoặc null
String getJsonField(const String &json,const String &key)
{
  String pattern = "\"" + key + "\"";
  int start = json.indexOf(pattern);
  if(start < 0) return "";
  start = json.indexOf(':',start + pattern.length());
  if(start < 0) return "";
  start++;
  while(start < json.length() && (json[start] == ' ' || json[start] == '\n' || json[start] == '\r' || json[start] == '\t'))
  {
    start++;
  }
  if(start >= json.length()) return "";
  if(json[start] == '"')
  {
    start++;
    String value;
    value.reserve(64);
    bool escaped = false;
    for(int i = start; i < json.length(); i++)
    {
      char c = json[i];
      if(escaped)
      {
        if(c == 'n') value += '\n';
        else if(c == 'r') value += '\r';
        else if(c == 't') value += '\t';
        else value += c;
        escaped = false;
        continue;
      }
      if(c == '\\')
      {
        escaped = true;
        continue;
      }
      if(c == '"') return value;
      value += c;
    }
    return "";
  }
  int end = start;
  while(end < json.length() && json[end] != ',' && json[end] != '}' && json[end] != '\n' && json[end] != '\r')
  {
    end++;
  }
  String value = json.substring(start,end);
  value.trim();
  return value;
}
// Chuyển ngày từ định dạng DD/MM/YYYY sang YYYY-MM-DD để lưu Firebase
String toIsoDate(String d)
{
  if(d.length() < 10) return "";
  return d.substring(6, 10) + "-" + d.substring(3, 5) + "-" + d.substring(0, 2);
}
// Mã hóa chuỗi theo định dạng application/x-www-form-urlencoded
// Được dùng khi gửi refresh token lên Firebase
String formUrlEncode(const String &input)
{
  const char hex[] = "0123456789ABCDEF";
  String output;
  output.reserve(input.length() * 3);
  for(size_t i = 0; i < input.length(); i++)
  {
    uint8_t c = (uint8_t)input[i];
    bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
    if(unreserved)
    {
      output += (char)c;
    }
    else if(c == ' ')
    {
      output += '+';
    }
    else
    {
      output += '%';
      output += hex[(c >> 4) & 0x0F];
      output += hex[c & 0x0F];
    }
  }
  return output;
}
// Tạo ngẫu nhiên mã kích hoạt gồm đúng 6 chữ số
String createActivationCode()
{
  uint32_t value = 100000UL + (esp_random() % 900000UL);
  return String(value);
}
// Kiểm tra chuỗi có đúng 6 ký tự số hay không
bool isSixDigitCode(const String &value)
{
  if(value.length() != 6) return false;
  for(uint8_t i = 0; i < 6; i++)
  {
    if(value[i] < '0' || value[i] > '9') return false;
  }
  return true;
}
// Kiểm tra chuỗi có phải số nguyên không âm và chỉ chứa chữ số hay không
bool isUnsignedNumber(const String &value)
{
  if(value.length() == 0) return false;
  for(size_t i = 0; i < value.length(); i++)
  {
    if(value[i] < '0' || value[i] > '9') return false;
  }
  return true;
}
// Cấp trước dung lượng cho các biến String dùng thường xuyên
// Giúp hạn chế phân mảnh vùng nhớ heap khi chương trình chạy lâu
void reserveStringMemory()
{
  uart_line.reserve(192);
  log_text.reserve(3072);
  lastLcd1FromSTM32.reserve(17);
  lastLcd2FromSTM32.reserve(17);
  lastBlynkLcd1.reserve(17);
  lastBlynkLcd2.reserve(17);
  last_user.reserve(16);
  last_action.reserve(8);
  last_floor.reserve(4);
  last_room.reserve(6);
  last_time.reserve(9);
  last_date.reserve(11);
  emergency_user.reserve(16);
  emergency_action.reserve(8);
  emergency_floor.reserve(4);
  emergency_room.reserve(6);
  emergency_time.reserve(9);
  emergency_date.reserve(11);
  firebaseIdToken.reserve(1400);
  firebaseRefreshToken.reserve(600);
  activeCommandId.reserve(32);
  activeCommandType.reserve(16);
  activeCommandUserId.reserve(4);
  activeCommandName.reserve(12);
  activeCommandVersion.reserve(6);
  activeCommandFloor.reserve(3);
  activeCommandRoom.reserve(5);
  activeCommandActivationCode.reserve(7);
  activeCommandDoneResult.reserve(32);
  activeCommandFailureResult.reserve(48);
}
// Cấu hình chế độ TLS cho kết nối Firebase và Telegram
// Hiện sử dụng setInsecure nên chưa xác minh chứng chỉ máy chủ
void configureTlsClients()
{
#if TLS_INSECURE_MODE
  firebaseClient.setInsecure();
  telegramClient.setInsecure();
  Serial.println("CẢNH BÁO: TLS KHÔNG XÁC THỰC CHỨNG CHỈ");
#else
#error "CHUA CAU HINH CHUNG CHI CA CHO FIREBASE VA TELEGRAM"
#endif
}
// Theo dõi trạng thái Wi-Fi, phát hiện mất hoặc khôi phục kết nối
// Tự thử kết nối lại và đặt lại trạng thái đồng bộ NTP khi cần
void processWiFiConnection()
{
  bool wifiConnected = WiFi.status() == WL_CONNECTED;
  if(wifiConnected)
  {
    if(!wifiWasConnected)
    {
      wifiWasConnected = true;
      ntpStarted = false;
      Serial.print("WIFI ĐÃ KẾT NỐI: ");
      Serial.println(WiFi.localIP());
    }
    return;
  }
  if(wifiWasConnected)
  {
    wifiWasConnected = false;
    ntpStarted = false;
    Serial.println("WIFI ĐANG MẤT KẾT NỐI");
  }
  if(millis() - wifiTick < WIFI_RETRY_INTERVAL) return;
  wifiTick = millis();
  Serial.println("ĐANG THỬ KẾT NỐI LẠI WIFI");
  WiFi.reconnect();
}
// Thử kết nối lại Blynk theo chu kỳ khi Wi-Fi đã sẵn sàng
// Không chặn vòng lặp chính trong thời gian dài
void processBlynkConnection()
{
  bool connected = Blynk.connected();
  if(connected)
  {
    if(!blynkWasConnected)
    {
      blynkWasConnected = true;
      Serial.println("ĐÃ KẾT NỐI BLYNK");
    }
    return;
  }
  if(blynkWasConnected)
  {
    blynkWasConnected = false;
    Serial.println("BLYNK MẤT KẾT NỐI");
  }
  if(WiFi.status() != WL_CONNECTED) return;
  if(millis() - blynkTick < BLYNK_RETRY_INTERVAL) return;
  blynkTick = millis();
  Serial.println("ĐANG THỬ KẾT NỐI LẠI BLYNK");
  if(Blynk.connect(1000))
  {
    blynkWasConnected = true;
    Serial.println("BLYNK ĐÃ KẾT NỐI");
  }
  else
  {
    Serial.println("KẾT NỐI LẠI BLYNK THẤT BẠI");
  }
}
// Gửi nội dung lên dòng LCD thứ nhất của Blynk
// Chỉ gửi khi nội dung thay đổi để tránh cập nhật dư thừa
void updateBlynkLCD1(const String &text)
{
  if (!Blynk.connected()) return;
  if (text == lastBlynkLcd1) return;
  lastBlynkLcd1 = text;
  Blynk.virtualWrite(V2, text);
}
// Gửi nội dung lên dòng LCD thứ hai của Blynk
// Chỉ gửi khi nội dung thay đổi để tránh cập nhật dư thừa
void updateBlynkLCD2(const String &text)
{
  if (!Blynk.connected()) return;
  if (text == lastBlynkLcd2) return;
  lastBlynkLcd2 = text;
  Blynk.virtualWrite(V3, text);
}
// Kiểm tra nội dung LCD có phải thông báo quan trọng cần gửi lên Blynk
// Dùng khi chế độ đồng bộ toàn bộ LCD đang tắt
bool isImportantLCDText(const String &text)
{
  return text.startsWith("CUA KHOA") ||
         text.startsWith("CUA DA MO") ||
         text.startsWith("LOI") ||
         text.startsWith("ERROR") ||
         text.startsWith("PHAT HIEN VAT") ||
         text.startsWith("CAN:CUA LUON MO") ||
         text.startsWith("KHONG CO VAT CAN") ||
         text.endsWith(" VAO") ||
         text.endsWith(" RA") ||
         text.indexOf("THANH CONG") >= 0 ||
         text.indexOf("THAT BAI") >= 0 ||
         text.indexOf("KHONG HOP LE") >= 0 ||
         text.indexOf("USER") >= 0;
}
// Tính thời điểm cần làm mới Firebase ID token.
// Chủ động làm mới sớm hơn thời gian hết hạn do Firebase cung cấp
void updateFirebaseTokenLifetime(const String &expiresText)
{
  long expiresSeconds = expiresText.toInt();
  if(expiresSeconds < 300)
  {
    expiresSeconds = 3600;
  }
  firebaseTokenLifetimeMs = ((unsigned long)expiresSeconds - 120UL) * 1000UL;
  firebaseTokenTick = millis();
}
// Đăng nhập tài khoản thiết bị Firebase bằng email và mật khẩu
// Lưu ID token, refresh token và kiểm tra đúng UID của thiết bị
bool firebaseLogin()
{
  if(WiFi.status() != WL_CONNECTED)
  {
    return false;
  }
  HTTPClient https;
  https.setConnectTimeout(5000);
  https.setTimeout(8000);
  String url ="https://identitytoolkit.googleapis.com/v1/""accounts:signInWithPassword?key=";
  url += FIREBASE_API_KEY;
  if(!https.begin(firebaseClient, url))
  {
    Serial.println("ĐĂNG NHẬP FIREBASE: KHỞI TẠO HTTPS THẤT BẠI");
    return false;
  }
  https.addHeader("Content-Type", "application/json");
  String body = "{";
  body += "\"email\":\"";
  body += jsonEscape(String(FIREBASE_DEVICE_EMAIL));
  body += "\",";
  body += "\"password\":\"";
  body += jsonEscape(String(FIREBASE_DEVICE_PASSWORD));
  body += "\",";
  body += "\"returnSecureToken\":true";
  body += "}";
  int httpCode = https.POST(body);
  String payload = https.getString();
  https.end();
  Serial.print("MÃ HTTP ĐĂNG NHẬP FIREBASE: ");
  Serial.println(httpCode);
  if(httpCode != 200)
  {
    String errorMessage = getJsonField(payload, "message");
    Serial.print("LỖI ĐĂNG NHẬP FIREBASE: ");
    Serial.println(errorMessage.length() > 0 ? errorMessage : "LỖI KHÔNG XÁC ĐỊNH");
    return false;
  }
  String newIdToken = getJsonField(payload, "idToken");
  String newRefreshToken = getJsonField(payload, "refreshToken");
  String newUid = getJsonField(payload, "localId");
  String expiresIn = getJsonField(payload, "expiresIn");
  if(newIdToken.length() == 0 || newRefreshToken.length() == 0 || newUid.length() == 0)
  {
    Serial.println( "ĐĂNG NHẬP FIREBASE THẤT BẠI: THIẾU TOKEN");
    return false;
  }
  if(newUid != String(FIREBASE_DEVICE_UID))
  {
    Serial.println("ĐĂNG NHẬP FIREBASE THẤT BẠI: UID THIẾT BỊ KHÔNG ĐÚNG");
    firebaseIdToken = "";
    firebaseRefreshToken = "";
    return false;
  }
  firebaseIdToken = newIdToken;
  firebaseRefreshToken = newRefreshToken;
  updateFirebaseTokenLifetime(expiresIn);
  Serial.println("ĐĂNG NHẬP FIREBASE THÀNH CÔNG");
  return true;
}
// Dùng refresh token để lấy Firebase ID token mới
// Cập nhật lại token và thời gian sử dụng sau khi làm mới thành công
bool firebaseRefreshIdToken()
{
  if(WiFi.status() != WL_CONNECTED)
  {
    return false;
  }
  if(firebaseRefreshToken.length() == 0)
  {
    return false;
  }
  HTTPClient https;
  https.setConnectTimeout(5000);
  https.setTimeout(8000);
  String url ="https://securetoken.googleapis.com/v1/""token?key=";
  url += FIREBASE_API_KEY;
  if(!https.begin(firebaseClient, url))
  {
    Serial.println("LÀM MỚI TOKEN FIREBASE: KHỞI TẠO HTTPS THẤT BẠI");
    return false;
  }
  https.addHeader("Content-Type","application/x-www-form-urlencoded");
  String body ="grant_type=refresh_token&refresh_token=";
  body += formUrlEncode(firebaseRefreshToken);
  int httpCode = https.POST(body);
  String payload = https.getString();
  https.end();
  Serial.print("MÃ HTTP LÀM MỚI TOKEN FIREBASE: ");
  Serial.println(httpCode);
  if(httpCode != 200)
  {
    String errorMessage =getJsonField(payload, "message");
    Serial.print("LỖI LÀM MỚI TOKEN FIREBASE: ");
    Serial.println(errorMessage.length() > 0 ? errorMessage : "LỖI KHÔNG XÁC ĐỊNH");
    return false;
  }
  String newIdToken =getJsonField(payload, "id_token");
  String newRefreshToken =getJsonField(payload, "refresh_token");
  String newUid =getJsonField(payload, "user_id");
  String expiresIn =getJsonField(payload, "expires_in");
  if(newIdToken.length() == 0 ||newUid.length() == 0)
  {
    Serial.println("LỖI LÀM MỚI TOKEN FIREBASE: PHẢN HỒI THIẾU TOKEN");
    return false;
  }
  if(newUid != String(FIREBASE_DEVICE_UID))
  {
    Serial.println("LỖI LÀM MỚI TOKEN FIREBASE: UID THIẾT BỊ KHÔNG ĐÚNG");
    firebaseIdToken = "";
    firebaseRefreshToken = "";
    return false;
  }
  firebaseIdToken = newIdToken;
  if(newRefreshToken.length() > 0)
  {
    firebaseRefreshToken = newRefreshToken;
  }
  updateFirebaseTokenLifetime(expiresIn);
  Serial.println("LÀM MỚI TOKEN FIREBASE THÀNH CÔNG");
  return true;
}
// Bảo đảm chương trình luôn có Firebase ID token còn hiệu lực
// Tự đăng nhập hoặc làm mới token khi token thiếu hoặc đã gần hết hạn
bool ensureFirebaseToken()
{
  if(firebaseIdToken.length() == 0)
  {
    if(firebaseRefreshToken.length() > 0)
    {
      if(firebaseRefreshIdToken())
      {
        return true;
      }
    }
    return firebaseLogin();
  }
  if((unsigned long)(millis() - firebaseTokenTick) >=
     firebaseTokenLifetimeMs)
  {
    if(firebaseRefreshIdToken())
    {
      return true;
    }
    firebaseIdToken = "";
    return firebaseLogin();
  }
  return true;
}
// Xóa toàn bộ thông tin và trạng thái của lệnh quản lý user hiện tại
// Đưa ESP32 về trạng thái sẵn sàng nhận lệnh Firebase tiếp theo
void clearActiveUserCommand()
{
  activeCommandId = "";
  activeCommandType = "";
  activeCommandUserId = "";
  activeCommandName = "";
  activeCommandVersion = "";
  activeCommandFloor = "";
  activeCommandRoom = "";
  activeCommandActivationCode = "";
  activeCommandDoneResult = "OK";
  activeCommandTick = 0;
  activeCommandSyncTick = 0;
  activeCommandHardwareDone = false;
  activeCommandRecovery = false;
  activeCommandFailed = false;
  activeCommandFailureResult = "";
}
// Gửi yêu cầu PATCH để cập nhật một phần command trên Firebase
// Tất cả hàm cập nhật trạng thái command dùng chung hàm này
bool patchFirebaseCommand(const String &body)
{
  if(!ensureFirebaseToken()) return false;
  String url;
  url.reserve(strlen(FIREBASE_DB_URL) + firebaseIdToken.length() + 64);
  url = String(FIREBASE_DB_URL) + "/device_commands/stm32_01.json?auth=" + firebaseIdToken;
  HTTPClient https;
  https.setConnectTimeout(3000);
  https.setTimeout(5000);
  if(!https.begin(firebaseClient,url)) return false;
  https.addHeader("Content-Type","application/json");
  int httpCode = https.PATCH(body);
  https.end();
  if(httpCode == 401) firebaseIdToken = "";
  return httpCode >= 200 && httpCode < 300;
}
// Ghi mã kích hoạt vừa tạo vào command đang chờ trên Firebase
bool updateFirebaseCommandActivationCode(const String &activationCode)
{
  String body;
  body.reserve(48);
  body = "{\"activation_code\":\"" + jsonEscape(activationCode) + "\"}";
  return patchFirebaseCommand(body);
}
// Cập nhật trạng thái và kết quả xử lý của command trên Firebase
bool updateFirebaseCommandStatus(const String &status,const String &result)
{
  String body;
  body.reserve(status.length() + result.length() + 40);
  body = "{\"status\":\"" + jsonEscape(status) + "\",\"result\":\"" + jsonEscape(result) + "\"}";
  return patchFirebaseCommand(body);
}
// Kiểm tra command mới từ Firebase, xác thực toàn bộ dữ liệu đầu vào
// Sau đó lưu trạng thái command và gửi lệnh USER_ADD hoặc USER_DELETE sang STM32
void processFirebaseUserCommand()
{
  if(WiFi.status() != WL_CONNECTED) return;
  if(activeCommandId.length() > 0) return;
  if(millis() - userCommandPollTick < USER_COMMAND_POLL_INTERVAL) return;
  userCommandPollTick = millis();
  if(!ensureFirebaseToken()) return;
  String url = String(FIREBASE_DB_URL) + "/device_commands/stm32_01.json?auth=" + firebaseIdToken;
  HTTPClient https;
  https.setConnectTimeout(3000);
  https.setTimeout(5000);
  if(!https.begin(firebaseClient,url)) return;
  int httpCode = https.GET();
  String payload = https.getString();
  https.end();
  if(httpCode != 200 || payload == "null") return;
  String status = getJsonField(payload,"status");
  String commandResult = getJsonField(payload,"result");
  status.trim();
  commandResult.trim();
  if(commandResult == "null") commandResult = "";
  if(status == "PROCESSING")
  {
    updateFirebaseCommandStatus("PENDING","ESP_RECOVER_RETRY");
    return;
  }
  if(status != "PENDING") return;
  String commandId = getJsonField(payload,"command_id");
  String type = getJsonField(payload,"type");
  String userId = getJsonField(payload,"user_id");
  String version = getJsonField(payload,"version");
  String name = getJsonField(payload,"name");
  String floor = getJsonField(payload,"floor");
  String room = getJsonField(payload,"room");
  String activationCode = getJsonField(payload,"activation_code");
  commandId.trim();
  type.trim();
  type.toUpperCase();
  userId.trim();
  version.trim();
  name.trim();
  floor.trim();
  room.trim();
  activationCode.trim();
  if(activationCode == "null") activationCode = "";
  if(commandId.length() == 0 || type.length() == 0 || userId.length() == 0 || version.length() == 0)
  {
    updateFirebaseCommandStatus("FAILED","BAD_COMMAND");
    return;
  }
  if(commandId.length() > 24 || commandId.indexOf('|') >= 0 || commandId.indexOf('\n') >= 0 || commandId.indexOf('\r') >= 0)
  {
    updateFirebaseCommandStatus("FAILED","INVALID_COMMAND_ID");
    return;
  }
  if(type != "USER_ADD" && type != "USER_DELETE")
  {
    updateFirebaseCommandStatus("FAILED","UNKNOWN_COMMAND");
    return;
  }
  if(!isUnsignedNumber(userId))
  {
    updateFirebaseCommandStatus("FAILED","INVALID_USER");
    return;
  }
  if(!isUnsignedNumber(version))
  {
    updateFirebaseCommandStatus("FAILED","INVALID_VERSION");
    return;
  }
  uint32_t userIdValue = (uint32_t)userId.toInt();
  uint32_t versionValue = (uint32_t)version.toInt();
  if(userIdValue < 1U || userIdValue > 150U)
  {
    updateFirebaseCommandStatus("FAILED","INVALID_USER");
    return;
  }
  if(versionValue < 1U || versionValue > 65535U)
  {
    updateFirebaseCommandStatus("FAILED","INVALID_VERSION");
    return;
  }
  userId = String(userIdValue);
  version = String(versionValue);
  bool commandNeedsName = type == "USER_ADD";
  if(commandNeedsName)
  {
    name.replace("|"," ");
    name.replace("\r"," ");
    name.replace("\n"," ");
    name.replace("\t"," ");
    name.trim();
    if(name.length() == 0 || name.length() > 12)
    {
      updateFirebaseCommandStatus("FAILED","INVALID_NAME");
      return;
    }
    if(!isUnsignedNumber(floor))
    {
      updateFirebaseCommandStatus("FAILED","INVALID_FLOOR");
      return;
    }
    if(!isUnsignedNumber(room))
    {
      updateFirebaseCommandStatus("FAILED","INVALID_ROOM");
      return;
    }
    uint32_t floorValue = (uint32_t)floor.toInt();
    uint32_t roomValue = (uint32_t)room.toInt();
    if(floorValue < 1U || floorValue > 99U)
    {
      updateFirebaseCommandStatus("FAILED","INVALID_FLOOR");
      return;
    }
    if(roomValue < 1U || roomValue > 9999U)
    {
      updateFirebaseCommandStatus("FAILED","INVALID_ROOM");
      return;
    }
    floor = String(floorValue);
    room = String(roomValue);
    if(activationCode.length() == 0)
    {
      activationCode = createActivationCode();
      if(!updateFirebaseCommandActivationCode(activationCode))
      {
        updateFirebaseCommandStatus("FAILED","ACTIVATION_CODE_CREATE_FAIL");
        return;
      }
    }
    if(!isSixDigitCode(activationCode))
    {
      updateFirebaseCommandStatus("FAILED","INVALID_ACTIVATION_CODE");
      return;
    }
  }
  String uartCommand = type + "|" + commandId + "|" + userId + "|" + version;
  if(commandNeedsName)
  {
    uartCommand += "|";
    uartCommand += name;
    uartCommand += "|";
    uartCommand += floor;
    uartCommand += "|";
    uartCommand += room;
    uartCommand += "|";
    uartCommand += activationCode;
  }
  if(uartCommand.length() >= 127)
  {
    updateFirebaseCommandStatus("FAILED","COMMAND_TOO_LONG");
    return;
  }
  activeCommandId = commandId;
  activeCommandType = type;
  activeCommandUserId = userId;
  activeCommandName = name;
  activeCommandVersion = version;
  activeCommandFloor = floor;
  activeCommandRoom = room;
  activeCommandActivationCode = activationCode;
  activeCommandDoneResult = "OK";
  activeCommandHardwareDone = false;
  activeCommandRecovery = commandResult == "ESP_RECOVER_RETRY" || commandResult == "STM32_TIMEOUT_RETRY";
  activeCommandFailed = false;
  activeCommandFailureResult = "";
  activeCommandSyncTick = 0;
  activeCommandTick = millis();
  if(!updateFirebaseCommandStatus("PROCESSING","SENT_TO_STM32"))
  {
    clearActiveUserCommand();
    return;
  }
  STM32.println(uartCommand);
  Serial.println("ĐÃ GỬI LỆNH QUẢN LÝ USER SANG STM32");
  if(commandNeedsName)
  {
    Serial.print("ĐÃ TẠO MÃ XÁC MINH CHO USER_ID ");
    Serial.println(userId);
  }
}
// Đồng bộ dữ liệu user trên Firebase sau khi STM32 xử lý phần cứng thành công
// USER_ADD sẽ tạo hoặc cập nhật user, USER_DELETE sẽ xóa user tương ứng
bool updateFirebaseUserFromCommand()
{
  if(!ensureFirebaseToken()) return false;
  String url = String(FIREBASE_DB_URL) + "/users/" + activeCommandUserId + ".json?auth=" + firebaseIdToken;
  HTTPClient https;
  https.setConnectTimeout(3000);
  https.setTimeout(5000);
  if(!https.begin(firebaseClient,url)) return false;
  https.addHeader("Content-Type","application/json");
  int userId = activeCommandUserId.toInt();
  int fingerId = userId - 1;
  int duressFingerId = fingerId + 150;
  int httpCode = -1;
  if(activeCommandType == "USER_ADD")
  {
    String body = "{\"name\":\"" + jsonEscape(activeCommandName) + "\",\"floor\":" + activeCommandFloor + ",\"room\":" + activeCommandRoom + ",\"status\":\"PENDING\",\"activation_code\":\"" + jsonEscape(activeCommandActivationCode) + "\",\"finger_id\":" + String(fingerId) + ",\"duress_finger_id\":" + String(duressFingerId) + ",\"version\":" + activeCommandVersion + "}";
    httpCode = https.PUT(body);
  }
  else if(activeCommandType == "USER_DELETE")
  {
    httpCode = https.sendRequest("DELETE");
  }
  https.end();
  Serial.print("MÃ HTTP ĐỒNG BỘ USER FIREBASE: ");
  Serial.println(httpCode);
  return httpCode >= 200 && httpCode < 300;
}
// Hoàn tất đồng bộ command sau khi đã nhận ACK thành công từ STM32
// Cập nhật dữ liệu user, chuyển command sang DONE rồi xóa trạng thái tạm
void syncActiveCommandToFirebase()
{
  if(activeCommandId.length() == 0) return;
  if(!activeCommandHardwareDone) return;
  if(WiFi.status() != WL_CONNECTED) return;
  activeCommandSyncTick = millis();
  if(!updateFirebaseUserFromCommand())
  {
    updateFirebaseCommandStatus("PROCESSING","FIREBASE_USER_SYNC_RETRY");
    return;
  }
  if(updateFirebaseCommandStatus("DONE",activeCommandDoneResult))
  {
    clearActiveUserCommand();
  }
}
// Thêm sự kiện thay đổi trạng thái user vào hàng đợi RAM
// Trả về false nếu hàng đợi đã đầy
bool enqueueUserEvent(const String &userId,const String &status,const String &version)
{
  if(userEventCount >= USER_EVENT_QUEUE_SIZE)
  {
    Serial.println("LỖI: HÀNG ĐỢI USER_EVENT ĐÃ ĐẦY");
    return false;
  }
  userEventQueue[userEventTail].userId = userId;
  userEventQueue[userEventTail].status = status;
  userEventQueue[userEventTail].version = version;
  userEventTail = (userEventTail + 1U) % USER_EVENT_QUEUE_SIZE;
  userEventCount++;
  return true;
}
// Gửi một USER_EVENT trong hàng đợi lên user tương ứng trên Firebase
// Đồng thời xóa mã kích hoạt sau khi user chuyển sang ACTIVE
bool sendUserEventToFirebase(const UserEventData &eventData)
{
  if(WiFi.status() != WL_CONNECTED) return false;
  if(!ensureFirebaseToken()) return false;
  String url = String(FIREBASE_DB_URL) + "/users/" + eventData.userId + ".json?auth=" + firebaseIdToken;
  String body = "{\"status\":\"" + jsonEscape(eventData.status) + "\",\"version\":" + eventData.version + ",\"activation_code\":null}";
  HTTPClient https;
  https.setConnectTimeout(3000);
  https.setTimeout(5000);
  if(!https.begin(firebaseClient,url)) return false;
  https.addHeader("Content-Type","application/json");
  int httpCode = https.PATCH(body);
  https.end();
  if(httpCode == 401)
  {
    firebaseIdToken = "";
  }
  return httpCode >= 200 && httpCode < 300;
}
// Gửi lần lượt các USER_EVENT đang chờ lên Firebase theo chu kỳ
// Chỉ xóa phần tử khỏi hàng đợi khi Firebase cập nhật thành công
void processUserEventQueue()
{
  if(userEventCount == 0) return;
  if(WiFi.status() != WL_CONNECTED) return;
  if(millis() - userEventSyncTick < USER_EVENT_SYNC_INTERVAL) return;
  userEventSyncTick = millis();
  if(!sendUserEventToFirebase(userEventQueue[userEventHead])) return;
  userEventQueue[userEventHead].userId = "";
  userEventQueue[userEventHead].status = "";
  userEventQueue[userEventHead].version = "";
  userEventHead = (userEventHead + 1U) % USER_EVENT_QUEUE_SIZE;
  userEventCount--;
  Serial.println("ĐỒNG BỘ USER EVENT THÀNH CÔNG");
}
// Tạo ID duy nhất cho một bản ghi ra vào
// ID gồm ngày, giờ và một giá trị ngẫu nhiên
String createEventId(const String &timeStr, const String &dateStr)
{
  String datePart = toIsoDate(dateStr);
  datePart.replace("-", "");
  String timePart = timeStr;
  timePart.replace(":", "");
  uint32_t randomPart = esp_random();
  char randomBuffer[9];
  snprintf(randomBuffer, sizeof(randomBuffer), "%08lX", (unsigned long)randomPart );
  return datePart + "_" + timePart + "_" + String(randomBuffer);
}
// Gửi một bản ghi ra vào hoàn chỉnh lên nhánh access_logs của Firebase
bool sendLogToFirebase(const String &eventId,const String &user,const String &action,const String &floor,const String &room,const String &timeStr,const String &dateStr)
{
  if(WiFi.status() != WL_CONNECTED)
  {
    return false;
  }
  if(!ensureFirebaseToken())
  {
    return false;
  }
  String url = String(FIREBASE_DB_URL);
  url += "/access_logs/";
  url += eventId;
  url += ".json?auth=";
  url += firebaseIdToken;
  String body = "{";
  body += "\"name\":\"" + jsonEscape(user) + "\",";
  body += "\"action\":\"" + jsonEscape(action) + "\",";
  body += "\"floor\":" + floor + ",";
  body += "\"room\":" + room + ",";
  body += "\"time\":\"" + jsonEscape(timeStr) + "\",";
  body += "\"date\":\"" + jsonEscape(dateStr) + "\",";
  body += "\"iso_date\":\"" + toIsoDate(dateStr) + "\",";
  body += "\"created_at\":\"";
  body += jsonEscape(timeStr + " - " + dateStr);
  body += "\"";
  body += "}";
  HTTPClient https;
  https.setConnectTimeout(3000);
  https.setTimeout(5000);
  if(!https.begin(firebaseClient, url))
  {
    return false;
  }
  https.addHeader("Content-Type", "application/json");
  int httpCode = https.PUT(body);
  String response = https.getString();
  https.end();
  Serial.print("MÃ HTTP GỬI LOG FIREBASE: ");
  Serial.println(httpCode);
  if(httpCode < 200 || httpCode >= 300)
  {
    Serial.print("LỖI GỬI LOG FIREBASE: ");
    Serial.println(response);
  }
  if(httpCode >= 200 && httpCode < 300)
  {
    return true;
  }
  if(httpCode == 401)
  {
    firebaseIdToken = "";
  }
  return false;
}
// Ghi một bản ghi ra vào vào LittleFS để chờ đồng bộ sau
// Mỗi trường được phân cách bằng ký tự tab
bool saveOfflineLog(const String &eventId,const String &user,const String &action,const String &floor,const String &room,const String &timeStr,const String &dateStr)
{
  File file = LittleFS.open(OFFLINE_LOG_FILE, FILE_APPEND);
  if(!file)
  {
    Serial.println("KHÔNG MỞ ĐƯỢC FILE LOG OFFLINE");
    return false;
  }
  file.print(eventId);
  file.print('\t');
  file.print(user);
  file.print('\t');
  file.print(action);
  file.print('\t');
  file.print(floor);
  file.print('\t');
  file.print(room);
  file.print('\t');
  file.print(timeStr);
  file.print('\t');
  file.println(dateStr);
  file.close();
  Serial.println("ĐÃ LƯU LOG OFFLINE");
  return true;
}
// Tách một dòng nhật ký offline thành các trường dữ liệu riêng
// Trả về false nếu dòng không đúng cấu trúc đã quy định
bool parseOfflineLog(const String &line,String &eventId,String &user,String &action,String &floor,String &room,String &timeStr,String &dateStr)
{
  int p1 = line.indexOf('\t');
  int p2 = line.indexOf('\t',p1 + 1);
  int p3 = line.indexOf('\t',p2 + 1);
  int p4 = line.indexOf('\t',p3 + 1);
  int p5 = line.indexOf('\t',p4 + 1);
  int p6 = line.indexOf('\t',p5 + 1);
  if(p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0 || p5 < 0 || p6 < 0)
  {
    return false;
  }
  eventId = line.substring(0,p1);
  user = line.substring(p1 + 1,p2);
  action = line.substring(p2 + 1,p3);
  floor = line.substring(p3 + 1,p4);
  room = line.substring(p4 + 1,p5);
  timeStr = line.substring(p5 + 1,p6);
  dateStr = line.substring(p6 + 1);
  eventId.trim();
  user.trim();
  action.trim();
  floor.trim();
  room.trim();
  timeStr.trim();
  dateStr.trim();
 return eventId.length() > 0 && user.length() > 0 && action.length() > 0 && isUnsignedNumber(floor) && isUnsignedNumber(room) && timeStr.length() > 0 && dateStr.length() > 0;
}
// Khôi phục file nhật ký tạm nếu quá trình ghi lại file bị gián đoạn
// Đồng thời xóa file tạm dư thừa khi file chính vẫn còn hợp lệ
bool recoverOfflineLogFile()
{
  if(LittleFS.exists(OFFLINE_LOG_FILE))
  {
    if(LittleFS.exists(OFFLINE_TEMP_FILE)) LittleFS.remove(OFFLINE_TEMP_FILE);
    return true;
  }
  if(!LittleFS.exists(OFFLINE_TEMP_FILE)) return true;
  if(!LittleFS.rename(OFFLINE_TEMP_FILE,OFFLINE_LOG_FILE))
  {
    Serial.println("KHÔNG PHỤC HỒI ĐƯỢC FILE LOG TẠM");
    return false;
  }
  Serial.println("ĐÃ PHỤC HỒI FILE LOG TẠM");
  return true;
}
// Đọc và gửi một nhóm nhật ký offline lên Firebase
// Các bản ghi chưa gửi được sẽ được giữ lại an toàn trong file mới
void processOfflineLogs()
{
  if(WiFi.status() != WL_CONNECTED) return;
  if(!recoverOfflineLogFile()) return;
  if(!LittleFS.exists(OFFLINE_LOG_FILE)) return;
  File source = LittleFS.open(OFFLINE_LOG_FILE,FILE_READ);
  if(!source) return;
  if(source.size() == 0)
  {
    source.close();
    LittleFS.remove(OFFLINE_LOG_FILE);
    return;
  }
  LittleFS.remove(OFFLINE_TEMP_FILE);
  File temp = LittleFS.open(OFFLINE_TEMP_FILE,FILE_WRITE);
  if(!temp)
  {
    source.close();
    return;
  }
  uint8_t sentCount = 0;
  bool stopSending = false;
  bool fileChanged = false;
  while(source.available())
  {
    String line = source.readStringUntil('\n');
    line.trim();
    if(line.length() == 0)
    {
      fileChanged = true;
      continue;
    }
    if(stopSending || sentCount >= OFFLINE_SYNC_BATCH_SIZE)
    {
      temp.println(line);
      continue;
    }
    String eventId;
    String user;
    String action;
    String floor;
    String room;
    String timeStr;
    String dateStr;
    if(!parseOfflineLog(line,eventId,user,action,floor,room,timeStr,dateStr))
    {
      Serial.println("BỎ LOG OFFLINE SAI ĐỊNH DẠNG");
      fileChanged = true;
      continue;
    }
    Serial.print("ĐANG ĐỒNG BỘ LOG OFFLINE: ");
    Serial.println(eventId);
    if(sendLogToFirebase(eventId,user,action,floor,room,timeStr,dateStr))
    {
      sentCount++;
      fileChanged = true;
      Serial.println("ĐỒNG BỘ LOG OFFLINE THÀNH CÔNG");
    }
    else
    {
      stopSending = true;
      temp.println(line);
    }
  }
  source.close();
  temp.close();
  if(!fileChanged)
  {
    LittleFS.remove(OFFLINE_TEMP_FILE);
    return;
  }
  File check = LittleFS.open(OFFLINE_TEMP_FILE,FILE_READ);
  if(!check)
  {
    Serial.println("KHÔNG KIỂM TRA ĐƯỢC FILE LOG TẠM");
    return;
  }
  bool tempHasData = check.size() > 0;
  check.close();
  if(!LittleFS.remove(OFFLINE_LOG_FILE))
  {
    Serial.println("KHÔNG XÓA ĐƯỢC FILE LOG CŨ");
    return;
  }
  if(!tempHasData)
  {
    LittleFS.remove(OFFLINE_TEMP_FILE);
    return;
  }
  if(!LittleFS.rename(OFFLINE_TEMP_FILE,OFFLINE_LOG_FILE))
  {
    Serial.println("KHÔNG ĐỔI TÊN ĐƯỢC FILE LOG TẠM");
    return;
  }
  Serial.print("SỐ LOG ĐÃ ĐỒNG BỘ TRONG ĐỢT: ");
  Serial.println(sentCount);
}
// Kiểm tra dữ liệu truy cập hiện tại và đưa bản ghi vào LittleFS
// CLOUD:PUSH từ STM32 sẽ gọi luồng xử lý này
void queueCurrentLog()
{
  if(last_user.length() == 0 || last_action.length() == 0 || last_floor.length() == 0 || last_room.length() == 0 || last_time.length() == 0 || last_date.length() == 0)
  {
    Serial.println("BỎ QUA CLOUD:PUSH VÌ THIẾU DỮ LIỆU");
    return;
  }
  String eventId = createEventId(last_time,last_date);
  if(saveOfflineLog(eventId,last_user,last_action,last_floor,last_room,last_time,last_date))
  {
    Serial.print("ĐÃ ĐƯA LOG VÀO HÀNG ĐỢI: ");
    Serial.println(eventId);
  }
  else
  {
    Serial.println("LỖI: KHÔNG LƯU ĐƯỢC LOG VÀO HÀNG ĐỢI");
  }
}
// Tạo và gửi thông báo truy cập khẩn cấp qua Telegram
// Trả về true khi Telegram chấp nhận yêu cầu gửi tin nhắn
bool sendTelegramAlert(const String &user,const String &action,const String &floor,const String &room,const String &timeStr,const String &dateStr)
{
  if(WiFi.status() != WL_CONNECTED)
  {
    Serial.println("KHÔNG GỬI TELEGRAM: MẤT WIFI");
    return false;
  }
  HTTPClient https;
  https.setConnectTimeout(3000);
  https.setTimeout(5000);
  String url = "https://api.telegram.org/bot";
  url += TELEGRAM_BOT_TOKEN;
  url += "/sendMessage";
  String msg;
  msg += "CẢNH BÁO\n\n";
  msg += "NGƯỜI DÙNG: " + user + "\n";
  msg += "TRẠNG THÁI: " + action + "\n";
  msg += "TẦNG: " + floor + "\n";
  msg += "PHÒNG: " + room + "\n";
  msg += "THỜI GIAN: " + timeStr + " - " + dateStr + "\n\n";
  msg += "HỆ THỐNG GHI NHẬN TRUY CẬP KHẨN CẤP";
  String body = "{";
  body += "\"chat_id\":\"";
  body += jsonEscape(String(TELEGRAM_CHAT_ID));
  body += "\",";
  body += "\"text\":\"";
  body += jsonEscape(msg);
  body += "\"";
  body += "}";
  if(!https.begin(telegramClient,url))
  {
    Serial.println("KHỞI TẠO KẾT NỐI TELEGRAM THẤT BẠI");
    return false;
  }
  https.addHeader("Content-Type","application/json");
  int httpCode = https.POST(body);
  Serial.print("MÃ HTTP TELEGRAM: ");
  Serial.println(httpCode);
  https.end();
  return httpCode >= 200 && httpCode < 300;
}
// Phân tích USER_ACK từ STM32 và đối chiếu với command đang xử lý
// Ghi nhận thành công, STALE phục hồi hoặc lỗi phần cứng tương ứng
bool handleUserAckLine(const String &line)
{
  if(!line.startsWith("USER_ACK|")) return false;
  int p1 = line.indexOf('|');
  int p2 = line.indexOf('|',p1 + 1);
  if(p1 < 0 || p2 < 0)
  {
    Serial.println("PHẢN HỒI USER_ACK SAI ĐỊNH DẠNG");
    return true;
  }
  String commandId = line.substring(p1 + 1,p2);
  String result = line.substring(p2 + 1);
  commandId.trim();
  result.trim();
  Serial.print("ID PHẢN HỒI=[");
  Serial.print(commandId);
  Serial.print("] LỆNH ĐANG CHỜ=[");
  Serial.print(activeCommandId);
  Serial.println("]");
  if(commandId != activeCommandId)
  {
    Serial.println("BỎ QUA PHẢN HỒI ACK KHÔNG KHỚP");
    return true;
  }
  if(result == "OK")
  {
    activeCommandHardwareDone = true;
    activeCommandFailed = false;
    activeCommandFailureResult = "";
    activeCommandDoneResult = "OK";
    activeCommandSyncTick = 0;
    return true;
  }
  if(result == "STALE" && activeCommandRecovery)
  {
    activeCommandHardwareDone = true;
    activeCommandFailed = false;
    activeCommandFailureResult = "";
    activeCommandDoneResult = "STALE_SYNCED";
    activeCommandSyncTick = 0;
    return true;
  }
  activeCommandHardwareDone = false;
  activeCommandFailed = true;
  activeCommandSyncTick = 0;
  if(result == "STALE") activeCommandFailureResult = "STALE_VERSION_MISMATCH";
  else if(result.length() > 0) activeCommandFailureResult = result;
  else activeCommandFailureResult = "UNKNOWN_ACK";
  return true;
}
// Phân tích USER_EVENT từ STM32, kiểm tra user ID, trạng thái và version
// Sự kiện hợp lệ sẽ được đưa vào hàng đợi đồng bộ Firebase
bool handleUserEventLine(const String &line)
{
  if(!line.startsWith("USER_EVENT|")) return false;
  int p1 = line.indexOf('|');
  int p2 = line.indexOf('|',p1 + 1);
  int p3 = line.indexOf('|',p2 + 1);
  if(p1 < 0 || p2 < 0 || p3 < 0)
  {
    Serial.println("USER_EVENT SAI ĐỊNH DẠNG");
    return true;
  }
  String userId = line.substring(p1 + 1,p2);
  String status = line.substring(p2 + 1,p3);
  String version = line.substring(p3 + 1);
  userId.trim();
  status.trim();
  version.trim();
  status.toUpperCase();
  if(status != "ACTIVE")
  {
    Serial.println("BỎ QUA USER_EVENT SAI TRẠNG THÁI");
    return true;
  }
  if(!isUnsignedNumber(userId) || !isUnsignedNumber(version))
  {
    Serial.println("BỎ QUA USER_EVENT SAI ĐỊNH DẠNG");
    return true;
  }
  uint32_t userIdValue = (uint32_t)userId.toInt();
  uint32_t versionValue = (uint32_t)version.toInt();
  if(userIdValue < 1U || userIdValue > 150U || versionValue < 1U || versionValue > 65535U)
  {
    Serial.println("BỎ QUA USER_EVENT NGOÀI PHẠM VI");
    return true;
  }
  enqueueUserEvent(String(userIdValue),status,String(versionValue));
  return true;
}
// Xử lý sự kiện duress và các cảnh báo không thể mở cửa từ STM32
// Sự kiện duress sẽ chờ CLOUD:PUSH để lấy đủ thông tin người truy cập
bool handleEmergencyLine(const String &line)
{
  if(line == "EVENT:EMERGENCY")
  {
    emergency_event_pending = true;
    emergency_event_tick = millis();
    return true;
  }
  if(line.startsWith("CANH BAO:"))
  {
    String warningText = line.substring(9);
    warningText.trim();
    updateBlynkLCD1("KHONG THE MO CUA");
    updateBlynkLCD2(warningText.length() > 0 ? warningText : "HE THONG DANG BAN");
    if(Blynk.connected()) Blynk.virtualWrite(V0,0);
    return true;
  }
  return false;
}
// Thu thập các dòng lịch sử gửi từ STM32 giữa LOG_START và LOG_END
// Sau khi nhận đủ sẽ hiển thị toàn bộ lịch sử lên Blynk
bool handleLogViewLine(const String &line)
{
  if(line == "LOG_START")
  {
    log_text = "LỊCH SỬ RA VÀO\n";
    return true;
  }
  if(line.startsWith("LOGVIEW:"))
  {
    if(log_text.length() < 3000)
    {
      log_text += line.substring(8);
      log_text += '\n';
    }
    return true;
  }
  if(line == "LOG_END")
  {
    if(Blynk.connected()) Blynk.virtualWrite(V12,log_text);
    log_text = "";
    return true;
  }
  return false;
}
// Thu thập các trường của một bản ghi ra vào từ STM32
// Khi nhận CLOUD:PUSH sẽ lưu log và chuẩn bị cảnh báo duress nếu có
bool handleAccessLogLine(const String &line)
{
  if(line.startsWith("LOG:"))
  {
    last_user = line.substring(4);
    if(Blynk.connected()) Blynk.virtualWrite(V6,last_user);
    return true;
  }
  if(line.startsWith("ACTION:"))
  {
    last_action = line.substring(7);
    if(Blynk.connected()) Blynk.virtualWrite(V7,last_action);
    return true;
  }
  if(line.startsWith("FLOOR:"))
  {
    last_floor = line.substring(6);
    if(Blynk.connected()) Blynk.virtualWrite(V8,last_floor);
    return true;
  }
  if(line.startsWith("ROOM:"))
  {
    last_room = line.substring(5);
    if(Blynk.connected()) Blynk.virtualWrite(V17,last_room);
    return true;
  }
  if(line.startsWith("TIME:"))
  {
    last_time = line.substring(5);
    if(Blynk.connected() && last_time != lastBlynkTime)
    {
      lastBlynkTime = last_time;
      Blynk.virtualWrite(V9,last_time);
    }
    return true;
  }
  if(line.startsWith("DATE:"))
  {
    last_date = line.substring(5);
    if(Blynk.connected() && last_date != lastBlynkDate)
    {
      lastBlynkDate = last_date;
      Blynk.virtualWrite(V10,last_date);
    }
    return true;
  }
  if(line == "CLOUD:PUSH")
  {
    queueCurrentLog();
    if(emergency_event_pending)
    {
      if(last_user.length() > 0 && last_action.length() > 0 && last_floor.length() > 0 && last_room.length() > 0 && last_time.length() > 0 && last_date.length() > 0)
      {
        emergency_user = last_user;
        emergency_action = last_action;
        emergency_floor = last_floor;
        emergency_room = last_room;
        emergency_time = last_time;
        emergency_date = last_date;
        emergency_request = true;
      }
      else
      {
        Serial.println("BỎ QUA CẢNH BÁO KHẨN VÌ THIẾU DỮ LIỆU");
      }
      emergency_event_pending = false;
    }
    return true;
  }
  return false;
}
// Nhận nội dung LCD1 và LCD2 từ STM32
// Chỉ đồng bộ nội dung cần thiết hoặc toàn bộ khi bật chế độ demo
bool handleLcdLine(const String &line)
{
  if(line.startsWith("LCD1:"))
  {
    String text = line.substring(5);
    lastLcd1FromSTM32 = text;
    if(demoSyncEnabled || isImportantLCDText(text)) updateBlynkLCD1(text);
    return true;
  }
  if(line.startsWith("LCD2:"))
  {
    String text = line.substring(5);
    lastLcd2FromSTM32 = text;
    if(demoSyncEnabled || isImportantLCDText(text)) updateBlynkLCD2(text);
    return true;
  }
  return false;
}
// Nhận trạng thái đóng hoặc mở cửa từ STM32
// Cập nhật đèn báo trạng thái cửa tương ứng trên Blynk
bool handleDoorLine(const String &line)
{
  if(line == "DOOR:OPEN")
  {
    if(Blynk.connected())
    {
      Blynk.virtualWrite(V5,255);
      Blynk.virtualWrite(V4,0);
    }
    return true;
  }
  if(line == "DOOR:CLOSE")
  {
    if(Blynk.connected())
    {
      Blynk.virtualWrite(V5,0);
      Blynk.virtualWrite(V4,255);
    }
    return true;
  }
  return false;
}
// Chuẩn hóa một dòng UART rồi chuyển tới handler phù hợp
// Mỗi dòng hợp lệ chỉ được xử lý bởi một nhóm giao thức
void processSTM32Line(String line)
{
  line.trim();
  if(line.length() == 0) return;
  Serial.print("STM32: ");
  Serial.println(line);
  if(handleUserAckLine(line)) return;
  if(handleUserEventLine(line)) return;
  if(handleEmergencyLine(line)) return;
  if(handleLogViewLine(line)) return;
  if(handleAccessLogLine(line)) return;
  if(handleLcdLine(line)) return;
  if(handleDoorLine(line)) return;
}
// Đọc UART từ STM32 theo từng ký tự và ghép thành từng dòng hoàn chỉnh
// Tự xóa bộ đệm nếu dữ liệu nhận vượt quá giới hạn cho phép
void readSTM32()
{
  while (STM32.available())
  {
    char c = STM32.read();
    if (c == '\n')
    {
      processSTM32Line(uart_line);
      uart_line = "";
    }
    else if (c != '\r')
    {
      if(uart_line.length() < 160)
      {
        uart_line += c;
      }
      else
      {
        uart_line = "";
      }
    }
  }
}
// Đọc thời gian hiện tại từ NTP rồi gửi TIME và DATE sang STM32
// Đồng thời cập nhật giờ và ngày trên Blynk khi dữ liệu thay đổi
bool sendTimeDateToSTM32()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo,200)) return false;
  if(timeinfo.tm_year < 125) return false;
  char timeBuf[16];
  char dateBuf[16];
  strftime(timeBuf,sizeof(timeBuf),"%H:%M:%S",&timeinfo);
  strftime(dateBuf,sizeof(dateBuf),"%d/%m/%Y",&timeinfo);
  String timeText(timeBuf);
  String dateText(dateBuf);
  STM32.print("TIME:");
  STM32.println(timeText);
  STM32.print("DATE:");
  STM32.println(dateText);
  if(Blynk.connected())
  {
    if(timeText != lastBlynkTime)
    {
      lastBlynkTime = timeText;
      Blynk.virtualWrite(V9,timeText);
    }
    if(dateText != lastBlynkDate)
    {
      lastBlynkDate = dateText;
      Blynk.virtualWrite(V10,dateText);
    }
  }
  return true;
}
// Nhận nút mở cửa V0 từ Blynk và gửi lệnh OPEN sang STM32
BLYNK_WRITE(V0)
{
  if (param.asInt() == 1)
  {
    Serial.println("ĐÃ GỬI LỆNH MỞ CỬA");
    STM32.print("OPEN\n");
  }
}
// Nhận nút reset V1 từ Blynk và gửi lệnh RESET sang STM32
BLYNK_WRITE(V1)
{
  if (param.asInt() == 1)
  {
    Serial.println("ĐÃ GỬI LỆNH RESET");
    STM32.print("RESET\n");
  }
}
// Nhận yêu cầu xem lịch sử tại V11 và yêu cầu STM32 gửi AccessLog
BLYNK_WRITE(V11)
{
  if(param.asInt())
  {
    STM32.print("GET_LOG\n");
  }
}
// Bật hoặc tắt chế độ đồng bộ toàn bộ nội dung LCD lên Blynk
// Khi vừa bật, hai dòng LCD gần nhất được gửi lên ngay lập tức
BLYNK_WRITE(V13)
{
  demoSyncEnabled = (param.asInt() == 1);
  Serial.print("CHẾ ĐỘ DEMO LCD: ");
  Serial.println(demoSyncEnabled ? "ON" : "OFF");
  if(demoSyncEnabled && Blynk.connected())
  {
    Blynk.virtualWrite(V2, lastLcd1FromSTM32);
    Blynk.virtualWrite(V3, lastLcd2FromSTM32);
    lastBlynkLcd1 = lastLcd1FromSTM32;
    lastBlynkLcd2 = lastLcd2FromSTM32;
  }
}
// Khởi tạo Serial, UART STM32, LittleFS, Wi-Fi, Blynk và tài nguyên hệ thống
void setup()
{
  Serial.begin(115200);
  reserveStringMemory();
  configureTlsClients();
  if(!LittleFS.begin(true))
  {
    Serial.println("KHỞI TẠO LITTLEFS THẤT BẠI");
  }
  else
  {
    Serial.println("LITTLEFS ĐÃ SẴN SÀNG");
  }
  STM32.setRxBufferSize(1024);
  STM32.begin(115200, SERIAL_8N1, STM_RX, STM_TX);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Blynk.config(BLYNK_AUTH_TOKEN);
}
// Vòng lặp điều phối toàn bộ hệ thống ESP32
// Xử lý UART, kết nối mạng, Firebase, NTP, log offline, telegram và command
void loop()
{
  readSTM32();
  processWiFiConnection();
  if (Blynk.connected())
  {
    Blynk.run();
  }
  processBlynkConnection();
  if(WiFi.status() == WL_CONNECTED)
  {
    processFirebaseUserCommand();
    processUserEventQueue();

    if(!ntpStarted)
    {
      configTime(gmtOffset_sec, daylightOffset_sec, "time.google.com", "pool.ntp.org", "time.cloudflare.com");
      ntpStarted = true;
      ntpRetryTick = millis();
      Serial.println("ĐÃ KHỞI ĐỘNG NTP");
    }

    if(!ntpSynchronized && millis() - ntpRetryTick >= NTP_RETRY_INTERVAL)
    {
      ntpRetryTick = millis();

      if(sendTimeDateToSTM32())
      {
        ntpSynchronized = true;
        timeSendTick = millis();
        Serial.println("NTP ĐỒNG BỘ THÀNH CÔNG");
      }
      else
      {
        Serial.println("NTP CHƯA ĐỒNG BỘ");
      }
    }
  }
  if(ntpSynchronized && millis() - timeSendTick >= TIME_SEND_INTERVAL)
  {
    if(sendTimeDateToSTM32())
    {
      timeSendTick = millis();
    }
    else
    {
      ntpSynchronized = false;
      ntpRetryTick = millis();
      Serial.println("THỜI GIAN HỆ THỐNG KHÔNG HỢP LỆ");
    }
  }
  if(WiFi.status() == WL_CONNECTED && millis() - offlineSyncTick >= OFFLINE_SYNC_INTERVAL)
  {
    offlineSyncTick = millis();
    processOfflineLogs();
  }
  if(emergency_event_pending && millis() - emergency_event_tick >= EMERGENCY_EVENT_TIMEOUT)
  {
    emergency_event_pending = false;
    Serial.println("HỦY EVENT:EMERGENCY VÌ KHÔNG NHẬN ĐƯỢC CLOUD:PUSH");
  }
  if(emergency_request && WiFi.status() == WL_CONNECTED && millis() - telegramRetryTick >= TELEGRAM_RETRY_INTERVAL)
  {
    telegramRetryTick = millis();
    if(sendTelegramAlert(emergency_user,emergency_action,emergency_floor,emergency_room,emergency_time,emergency_date))
    {
      emergency_request = false;
      emergency_user = "";
      emergency_action = "";
      emergency_floor = "";
      emergency_room = "";
      emergency_time = "";
      emergency_date = "";
    }
  }
  if(activeCommandId.length() > 0 && activeCommandFailed && millis() - activeCommandSyncTick >= USER_DATABASE_SYNC_INTERVAL)
  {
    activeCommandSyncTick = millis();

    if(updateFirebaseCommandStatus("FAILED",activeCommandFailureResult))
    {
      clearActiveUserCommand();
    }
  }
  if(activeCommandId.length() > 0 && !activeCommandHardwareDone && !activeCommandFailed && millis() - activeCommandTick >= USER_COMMAND_TIMEOUT)
  {
    if(updateFirebaseCommandStatus("PENDING","STM32_TIMEOUT_RETRY"))
    {
      clearActiveUserCommand();
    }
    else
    {
      activeCommandTick = millis();
    }
  }
  if(activeCommandId.length() > 0 && activeCommandHardwareDone && millis() - activeCommandSyncTick >= USER_DATABASE_SYNC_INTERVAL)
  {
    syncActiveCommandToFirebase();
  }
}