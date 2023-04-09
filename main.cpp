//
// copyright wave.suzuki.z@gmail.com
// since 2021
//

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{

/// ファイルヘッダ
struct header
{
  uint32_t magic   = 0x544e4f46; // "FONT"
  uint16_t num     = 0;          // 収録文字数
  uint8_t  size    = 0;          // フォントサイズ
  uint8_t  reserve = 0;
};

/// フォントデータ
struct hint
{
  uint32_t dataOffset;
  uint16_t code;
  uint8_t  width;
  uint8_t  height;
  uint8_t  pitch;
  int8_t   offsetX;
  int8_t   offsetY;
  uint8_t  byteStep;
};

/// フォントコンバート情報
struct font
{
  uint32_t              code;
  std::string           utf8;
  std::vector<uint64_t> bitmap;
  // hint
  uint8_t width;  // 描画幅
  uint8_t height; // 描画高さ
  uint8_t pitch;  // 空間幅
  int8_t  offsetX;
  int8_t  offsetY;
  size_t  byteStep; // 横方向のバイト数(8px=1,16px=2,32px=4...)

  size_t dataOffset;

  bool operator==(const std::string& str) const { return utf8 == str; }
  bool operator==(uint32_t c) const { return code == c; }
  bool operator<(const font& other) const { return code < other.code; }
};

///
void
convcode(const char*& ptr, uint32_t& code)
{
  uint32_t c0 = *ptr;
  if (c0 <= 0x7f)
  {
    // ascii
    code = c0;
    ptr++;
    return;
  }

  switch (c0 & 0xf0)
  {
  case 0xc0:
  case 0xd0:
    code = (c0 & 0x1f) << 6 | (ptr[1] & 0x3f);
    ptr += 2;
    break;
  case 0xe0:
    code = (c0 & 0xf) << 12 | (ptr[1] & 0x3f) << 6 | (ptr[2] & 0x3f);
    ptr += 3;
    break;
  case 0xf0:
    code = (c0 & 0x7) << 18 | (ptr[1] & 0x3f) << 12 | (ptr[2] & 0x3f) << 6 | (ptr[3] & 0x3f);
    ptr += 4;
    break;
  default:
    break;
  }
}

///
std::string
convert(uint32_t c)
{
  char buff[5]{};
  int  len = 1;
  // 0000 0000-0000 007F | 0xxxxxxx
  // 0000 0080-0000 07FF | 110xxxxx 10xxxxxx
  // 0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
  // 0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
  if (c <= 0x7f)
  {
    buff[0] = c;
  }
  else if (c <= 0x7ff)
  {
    buff[0] = 0xc0 | (c >> 6);
    buff[1] = 0x80 | (c & 0x3f);
    len     = 2;
  }
  else if (c <= 0xffff)
  {
    buff[0] = 0xe0 | (c >> 12);
    buff[1] = 0x80 | ((c >> 6) & 0x3f);
    buff[2] = 0x80 | ((c & 0x3f));
    len     = 3;
  }
  return buff;
}

/// 文字列nを文字sで区切って配列にして返す
/// 適当に切り貼りするので長い文字列には使用しない(遅い)
std::vector<std::string>
split(const std::string& n, const char s = ' ')
{
  std::vector<std::string> l;

  size_t pos = 0;
  do
  {
    auto p = n.find(s, pos);
    if (p == std::string::npos)
      break;
    l.emplace_back(n.substr(pos, p - pos));
    pos = p + 1;
  } while (true);
  l.emplace_back(n.substr(pos));

  return l;
}

/// 文字列nがsで始まるか調べて、その通りなら残りの文字列をargに入れて返す
bool
cmp(const std::string& n, const char* s, std::string& arg)
{
  int l = std::strlen(s);
  if (n.length() < l)
  {
    arg = "";
    return false;
  }

  for (int i = 0; i < l; i++)
  {
    if (n[i] != s[i])
    {
      arg = "";
      return false;
    }
  }
  arg = n.substr(l);
  return true;
};

/// 一行切り抜き
size_t
cut(const std::string& n, size_t pos, std::string& ret)
{
  auto p = n.find('\n', pos);
  ret    = p != std::string::npos ? n.substr(pos, p - pos) : "";
  return p;
}

} // namespace

int
main(int argc, char** argv)
{
  if (argc < 4)
  {
    std::cerr << argv[0] << ": need bdf-file out-file use-char-list(or .txt)" << std::endl;
    return -1;
  }

  // bdfファイル読み込み
  std::filesystem::path fname{argv[1]};
  if (fname.extension() != ".bdf")
  {
    std::cerr << argv[0] << ": not bdf" << std::endl;
    return -1;
  }
  if (std::filesystem::exists(fname) == false)
  {
    std::cerr << "bdf not found: " << fname << std::endl;
    return -1;
  }
  std::ifstream infile{fname};
  infile.seekg(0, std::ios_base::end);
  int filesize = infile.tellg();
  infile.seekg(0, std::ios_base::beg);
  std::cout << "filesize: " << filesize << std::endl;
  std::string buffer;
  buffer.resize(filesize);
  infile.read(buffer.data(), buffer.size());

  // 使用文字ファイル
  std::string useCharList;
  if (strcmp(argv[3], "-") == 0)
  {
    // 直接指定
    useCharList = argv[4];
  }
  else
  {
    // ファイルから読む
    fname = argv[3];
    if (std::filesystem::exists(fname) == false)
    {
      std::cerr << ".txt file not found: " << fname << std::endl;
      return -1;
    }
    std::ifstream ufile{fname};
    ufile.seekg(0, std::ios_base::end);
    filesize = ufile.tellg();
    ufile.seekg(0, std::ios_base::beg);
    useCharList.resize(filesize);
    ufile.read(useCharList.data(), useCharList.size());
  }
  std::vector<uint32_t> useCodeList;
  useCodeList.reserve(useCharList.size());
  for (const char* ptr = useCharList.c_str(); *ptr != '\0';)
  {
    uint32_t code;
    convcode(ptr, code);
    useCodeList.push_back(code);
  }

  //
  size_t      fontSize   = 16;
  size_t      ascentSize = 0;
  size_t      count      = 0;
  std::string encode     = "ascii";

  // プロパティを読む
  size_t pos = 0;
  do
  {
    std::string line, arg;
    if (cut(buffer, pos, line) == std::string::npos)
    {
      std::cerr << "invalid format!" << std::endl;
      return -1;
    }
    pos += line.length() + 1;
    if (cmp(line, "PIXEL_SIZE ", arg))
      fontSize = std::stoi(arg);
    else if (cmp(line, "CHARSET_REGISTRY ", arg))
      encode = arg;
    else if (cmp(line, "FONT_ASCENT ", arg))
      ascentSize = std::stoi(arg);
    else if (cmp(line, "ENDPROPERTIES", arg))
      break;
  } while (true);
  {
    // 文字数取得
    std::string line, arg;
    if (cut(buffer, pos, line) != std::string::npos)
    {
      if (cmp(line, "CHARS ", arg))
        count = std::stoi(arg);
    }
    pos += line.length() + 1;
  }
  // info
  std::cout << "文字コード: " << encode << std::endl;
  std::cout << "フォントサイズ: " << fontSize << std::endl;
  std::cout << "収録文字数: " << count << std::endl;

  std::vector<font> fontList;
  fontList.reserve(count);

  font current;
  current.bitmap.resize(fontSize);

  //
  int cnt      = 0;
  int onBitmap = -1;
  while (true)
  {
    std::string line, arg;
    if (cut(buffer, pos, line) == std::string::npos)
      break;

    if (cmp(line, "STARTCHAR ", arg))
    {
      cnt++;
    }
    else if (cmp(line, "ENCODING ", arg))
    {
      current.code = std::stoi(arg);
      current.utf8 = convert(current.code);
    }
    else if (cmp(line, "BITMAP", arg))
    {
      onBitmap = 0;
    }
    else if (cmp(line, "ENDCHAR", arg))
    {
      onBitmap = -1;
      if (std::find(useCodeList.begin(), useCodeList.end(), current.code) != useCodeList.end())
        fontList.push_back(current);
    }
    else if (cmp(line, "BBX ", arg))
    {
      // バウンディングボックス
      auto l           = split(arg);
      current.width    = std::stoi(l[0]);
      current.height   = std::stoi(l[1]);
      current.offsetX  = std::stoi(l[2]);
      int ascent       = std::stoi(l[3]);
      current.offsetY  = ascentSize - (current.height + ascent);
      current.byteStep = (current.width + 7) >> 3;
    }
    else if (cmp(line, "DWIDTH ", arg))
    {
      // 実際の描画幅
      auto l        = split(arg);
      current.pitch = std::stoi(l[0]);
    }
    else if (onBitmap >= 0)
    {
      current.bitmap[onBitmap++] = std::stoull(line, nullptr, 16);
    }

    pos += line.length() + 1;
  }
  std::cout << "find: " << fontList.size() << std::endl;

  std::sort(fontList.begin(), fontList.end());

  // ファイル出力
  std::filesystem::path ofname{argv[2]};
  std::ofstream         outfile{ofname, std::ios_base::binary};
  header                h;
  h.num  = fontList.size();
  h.size = fontSize;
  outfile.write((const char*)&h, sizeof(h));
  size_t dataOffset = sizeof(h) + fontList.size() * sizeof(hint);
  for (auto& f : fontList)
  {
    // インデックス
    hint hint{};
    hint.dataOffset = dataOffset;
    hint.code       = f.code;
    hint.width      = f.width;
    hint.height     = f.height;
    hint.pitch      = f.pitch;
    hint.offsetX    = f.offsetX;
    hint.offsetY    = f.offsetY;
    hint.byteStep   = f.byteStep;
    outfile.write((const char*)&hint, sizeof(hint));
    f.dataOffset = dataOffset;
    dataOffset += f.byteStep * f.height;
  }
  std::vector<uint8_t> bitmapBuffer;
  bitmapBuffer.resize(((fontSize + 7) >> 3) * fontSize);

  for (auto& f : fontList)
  {
    // ビットマップ
    uint8_t* b = bitmapBuffer.data();
    for (int height = 0; height < f.height; height++)
    {
      auto line = f.bitmap[height];
      for (int width = 0; width < f.byteStep; width++)
      {
        *(b++) = line & 0xff;
        line >>= 8ULL;
      }
    }
    size_t wsz = f.byteStep * f.height;
    outfile.write((const char*)bitmapBuffer.data(), wsz);
  }
  auto dataSize = outfile.tellp();
  std::cout << "output " << ofname << " size: " << dataSize;
  std::cout << std::hex << "(" << dataOffset << ")";
  std::cout << std::dec << std::endl;

  return 0;
}
