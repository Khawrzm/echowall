# دليل ECHOWALL — واجهة المطور العربية

## نظرة عامة

ECHOWALL هو نظام رادار سلبي مفتوح المصدر يكشف وجود الأشخاص خلف الجدران باستخدام إشارات Wi-Fi الموجودة أصلاً، دون كاميرات أو أجهزة استشعار خاصة.

## التثبيت

```bash
pip install echowall
```

## الاستخدام السريع

```python
from echowall import EchowallPipeline

# تهيئة النظام في وضع المحاكاة
pipeline = EchowallPipeline(mode="sim")

# تشغيل دورة واحدة
result = pipeline.get_result()

if result and result.presence:
    print(f"✅ يوجد {result.count} شخص")
    print(f"الوضعية: {result.posture}")
    print(f"الثقة: {result.confidence:.0%}")
else:
    print("❌ لا يوجد أحد")
```

## واجهة REST API

بعد تشغيل `echowall run`، تصبح الواجهة متاحة على:

```
GET http://localhost:8765/presence
```

### مثال باستخدام Python requests

```python
import requests

response = requests.get("http://localhost:8765/presence")
data = response.json()

print(f"الوجود: {'نعم' if data['presence'] else 'لا'}")
print(f"العدد: {data['count']} شخص")
print(f"الوضعية: {data['posture']}")
```

## WebSocket للبث المباشر

```javascript
const ws = new WebSocket('ws://localhost:8765/ws');

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log(`الوجود: ${data.presence ? 'نعم' : 'لا'}`);
  console.log(`العدد: ${data.count}`);
};
```

## الأجهزة المدعومة

| الجهاز | الحالة |
|--------|--------|
| ESP32-S3 | ✅ مستقر |
| Raspberry Pi 4 | ✅ مستقر |
| Linux + Intel Wi-Fi 6 | 🔄 تجريبي |
| محاكاة (بدون جهاز) | ✅ متاح |

## الخصوصية

ECHOWALL لا يرسل أي بيانات إلى الخارج. كل المعالجة تتم محلياً على الجهاز. الواجهة الخارجية تُعيد فقط:
- هل يوجد أحد؟ (نعم/لا)
- كم عددهم؟
- ما وضعيتهم؟

لا تُكشف بيانات الإشارة الخام أبداً.
