/*
 *  Copyright(C) by Yukiya Ishioka
 */

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>     /* RTOS task related API prototypes. */
#include <freertos/timers.h>
#include <freertos/event_groups.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>

/******************************************************/
/* Wi-Fiアクセスポイントの SSID とパスフレーズ        */
/* 利用する環境のSSIDとパスフレーズを設定してください */
/******************************************************/
const char* ssid1     = "xxxxxxxxxx";
const char* password1 = "xxxxxxxxxx";
const char* ssid2     = "xxxxxxxxxx";
const char* password2 = "xxxxxxxxxx";
/******************************************************/

#define DEF_MSG_UPDATE      (1000 * 60 * 20)
#define DEF_MSG_NUM_MAX     15

// https://news.yahoo.co.jp/rss
// https://news.ntv.co.jp/rss/index.rdf
// https://rss.wor.jp/
// https://www.asahi.com/information/service/rss.html
// https://www.nhk.or.jp/toppage/rss/index.html

#if 1
const char *url[] = {
  "https://pc.watch.impress.co.jp/data/rss/1.0/pcw/feed.rdf",
  "https://news.yahoo.co.jp/rss/topics/domestic.xml",
  "https://news.yahoo.co.jp/rss/topics/business.xml",
  "https://assets.wor.jp/rss/rdf/sankei/flash.rdf",
  "https://assets.wor.jp/rss/rdf/yomiuri/latestnews.rdf",
  "https://assets.wor.jp/rss/rdf/bloomberg/top.rdf",
  "https://www.asahi.com/rss/asahi/newsheadlines.rdf",
  "https://www.asahi.com/rss/asahi/national.rdf",
  "https://www.asahi.com/rss/asahi/politics.rdf",
  "https://www.asahi.com/rss/asahi/business.rdf",
  "https://news.yahoo.co.jp/rss/topics/world.xml",
  "https://news.yahoo.co.jp/rss/topics/sports.xml",
  "https://news.yahoo.co.jp/rss/topics/it.xml",
  "https://news.yahoo.co.jp/rss/topics/science.xml",
  "https://news.yahoo.co.jp/rss/topics/local.xml",
  NULL  /* 終了マーク */
};
#endif


#define BTNPIN     4
#define VOLPIN    34
#define LEDPIN     2

#define S1NPIN    12
#define S2NPIN    13
#define S3NPIN    14
#define CLKPIN    15
#define LATPIN    16


unsigned char  msg_test[] = "システム初期化中";

extern  const unsigned char  fx_8x16rk_fnt[];
extern  const unsigned char  jiskan16_fnt[];
extern  const unsigned char  Utf8Sjis_tbl[];

#define DEF_FONT_SIZE        16
#define DEF_FONT_A16_VAR     fx_8x16rk_fnt
#define DEF_FONT_K16_VAR     jiskan16_fnt

#define DEF_LED_WIDTH        16
#define DEF_LED_HIGHT        16
#define DEF_MOD_WIDTH        (DEF_LED_WIDTH * 2)
#define DEF_MOD_NUM          4
#define DEF_MODT_NUM         3

#define DEF_OUTDATA_NUM      3000
#define DEF_OUTTITLE_NUM     (DEF_MOD_WIDTH*DEF_MODT_NUM + 60*8 + DEF_MOD_WIDTH*DEF_MODT_NUM)
#define DEF_CHAR_NUM         ((DEF_OUTDATA_NUM - (DEF_MOD_WIDTH * DEF_MOD_NUM)*2) /8)

unsigned char  dat_buffer[ DEF_FONT_SIZE ][ DEF_OUTDATA_NUM ];
unsigned char  datt_buffer[ DEF_FONT_SIZE ][ DEF_OUTTITLE_NUM ];
unsigned char  adr_tbl[ DEF_LED_HIGHT ][ DEF_LED_WIDTH ];
unsigned char  msg_buffer[ DEF_MSG_NUM_MAX ][ DEF_OUTDATA_NUM / 8 * 2 + 1 ];
unsigned char  msgt_buffer[ DEF_MSG_NUM_MAX ][ DEF_OUTTITLE_NUM / 8 * 2 + 1 ];
int  led_msg_buf_len[ DEF_MSG_NUM_MAX ];
int  led_msgt_buf_len[ DEF_MSG_NUM_MAX ];
int  led_msg_len ;
int  led_pos;
int  led_msgt_len;
int  led_msgt_pos;
int  msg_reload_flag;
int  msg_chg_flag;
int  sys_init_flag;

TaskHandle_t  thand_mainTask;
TaskHandle_t  thand_slideTask;
TaskHandle_t  thand_wifiTask;
TimerHandle_t thand_timeMsg;
EventGroupHandle_t  ehandMsg;


static volatile  int  usec_cnt;

void  usec_delay( int usec )
{
    int  cnt;

    if( usec <= 2 ) {
        usec = 3;
    }
    cnt = usec;

    for( usec_cnt=0; usec_cnt< cnt ; usec_cnt++ ) ;
}


void  dev_led_clr_out( void )
{
    int  tmpcol;
    int  num;

  for( num=0 ; num<DEF_MOD_NUM ; num++ ) {
    for( tmpcol=0 ; tmpcol<16 ; tmpcol++ ) {

        digitalWrite( S1NPIN, adr_tbl[0][tmpcol] );
        digitalWrite( S2NPIN, 0 );
        digitalWrite( S3NPIN, 0 );

        usec_delay( 1 );
        digitalWrite( CLKPIN, 1 );
        usec_delay( 1 );
        digitalWrite( CLKPIN, 0 );
    }
  }
    digitalWrite( LATPIN, 0 );
    usec_delay( 1 );
    digitalWrite( LATPIN, 1 );
}


/*
 *  clear message display buffer
 */
void  clear_message( void )
{
  memset( dat_buffer, 0x00, sizeof( dat_buffer ) );
}


/*
 *  clear title display buffer
 */
void  clear_title( void )
{
  memset( datt_buffer, 0x00, sizeof( datt_buffer ) );
}


/*
 *  set font data to display buffer
 */
void  set_font( unsigned char *font, unsigned char *buff, int pos, int width )
{
    int  i, j, k;
    int  row;
    int  w = (width/8);   /* font width byte */
    unsigned char  pat;

    /* row */
    for( i=0 ; i<DEF_FONT_SIZE ; i++ ) {
        row = DEF_OUTDATA_NUM * i;
        /* col */
        for( j=0 ; j<w ; j++ ) {
            pat = 0x80;
            for( k=0 ; k<8 ; k++ ) {
                if( (font[ i * w + j ] & pat) != 0 ) {
                    /*         base up/low  offset */
                    buff[ row + pos + j*8 + k ] = 1;
                }
                pat >>= 1; /* bit shift */
            }
        }
    }
}


void  set_font_title( unsigned char *font, unsigned char *buff, int pos, int width )
{
    int  i, j, k;
    int  row;
    int  w = (width/8);   /* font width byte */
    unsigned char  pat;

    /* row */
    for( i=0 ; i<DEF_FONT_SIZE ; i++ ) {
        row = DEF_OUTTITLE_NUM * i;
        /* col */
        for( j=0 ; j<w ; j++ ) {
            pat = 0x80;
            for( k=0 ; k<8 ; k++ ) {
                if( (font[ i * w + j ] & pat) != 0 ) {
                    /*         base up/low  offset */
                    buff[ row + pos + j*8 + k ] = 1;
                }
                pat >>= 1; /* bit shift */
            }
        }
    }
}


void UTF8_To_SJIS_cnv(unsigned char utf8_1, unsigned char utf8_2, unsigned char utf8_3, unsigned int* spiffs_addrs)
{
  unsigned int  UTF8uint = utf8_1*256*256 + utf8_2*256 + utf8_3;
   
  if(utf8_1>=0xC2 && utf8_1<=0xD1){
    *spiffs_addrs = ((utf8_1*256 + utf8_2)-0xC2A2)*2 + 0xB0;
  }else if(utf8_1==0xE2 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE28090)*2 + 0x1EEC;
  }else if(utf8_1==0xE3 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE38080)*2 + 0x9DCC;
  }else if(utf8_1==0xE4 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE4B880)*2 + 0x11CCC;
  }else if(utf8_1==0xE5 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE58085)*2 + 0x12BCC;
  }else if(utf8_1==0xE6 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE6808E)*2 + 0x1AAC2;
  }else if(utf8_1==0xE7 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE78081)*2 + 0x229A6;
  }else if(utf8_1==0xE8 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE88080)*2 + 0x2A8A4;
  }else if(utf8_1==0xE9 && utf8_2>=0x80){
    *spiffs_addrs = (UTF8uint-0xE98080)*2 + 0x327A4;
  }else if(utf8_1>=0xEF && utf8_2>=0xBC){
    *spiffs_addrs = (UTF8uint-0xEFBC81)*2 + 0x3A6A4;
  }
}


int  utf8_to_sjis( unsigned char *buff, unsigned char *code1, unsigned char *code2 )
{
  unsigned char  utf8_1, utf8_2, utf8_3;
  unsigned char  sjis[2];
  unsigned int  sp_addres;
  int  pos = 0;
  int fnt_cnt = 0;
 
  if(buff[pos]>=0xC2 && buff[pos]<=0xD1){
    /* UTF8 2bytes */
    utf8_1 = buff[pos];
    utf8_2 = buff[pos+1];
    utf8_3 = 0x00;
    fnt_cnt = 2;
  }else if(buff[pos]>=0xE2 && buff[pos]<=0xEF){
    /* UTF8 3bytes */
    utf8_1 = buff[pos];
    utf8_2 = buff[pos+1];
    utf8_3 = buff[pos+2];
    fnt_cnt = 3;
  }else{
    utf8_1 = buff[pos];
    utf8_2 = 0x00;
    utf8_3 = 0x00;
    fnt_cnt = 1;
  }

  /* UTF8 to Sjis change table position */
  UTF8_To_SJIS_cnv( utf8_1, utf8_2, utf8_3, &sp_addres );
  *code1 = Utf8Sjis_tbl[ sp_addres   ];
  *code2 = Utf8Sjis_tbl[ sp_addres+1 ];

  return  fnt_cnt;
}


unsigned char  *get_fontx2_a( unsigned char *font, unsigned int code )
{
    unsigned char  *address = NULL ;
    unsigned int  fontbyte ;

    fontbyte = (font[14] + 7) / 8 * font[15] ;
    address = &font[17] + fontbyte * code ;

    return  address ;
}


unsigned char  *get_fontx2_k( unsigned char *font, unsigned int code )
{
    unsigned char  *address = NULL ;
    unsigned char  *tmp ;
    unsigned int  blknum, i, fontnum ;
    unsigned int  bstart, bend ;
    unsigned int  fontbyte ;

    fontbyte = (font[14] + 7) / 8 * font[15] ;
    fontnum = 0 ;

    blknum = (unsigned int)font[17] * 4 ;
    tmp = &font[18] ;
    for( i=0 ; i<blknum ; i+=4 ) {
        bstart = tmp[i]   + ((unsigned int)tmp[i+1] << 8) ;
        bend   = tmp[i+2] + ((unsigned int)tmp[i+3] << 8) ;
        if( code >= bstart && code <= bend ) {
            address = tmp + (fontnum + (code - bstart)) * fontbyte + blknum ;
            break ;
        }

        fontnum += (bend - bstart) + 1 ;
    }

    if( address == 0 ) {
        Serial.printf( "*get_fontx2_k( 0x%X ) : address = %X \n", code, address );
        if( code == 0x8780 ) {
            code = 0x8168;
        } else if( code == 0x8781 ) {
            code = 0x8168;
        } else {
            code = 0x8145; /* if there is no data, "・" */
        }
        address = get_fontx2_k( font, code );
    }

    return  address ;
}


int  make_message( unsigned char *strbuff, unsigned int size )
{
    int  pos, bitpos;
    int  num;
    unsigned char  *fontdata;
    unsigned int  code;
    unsigned char  code1, code2 ;

    clear_message();

    pos = 0 ;
    bitpos = (DEF_MOD_WIDTH * DEF_MOD_NUM) ;

    while( pos < size ) {
        code = strbuff[ pos ] ;  /* get 1st byte */
        if( code < 0x80 ) {
            /* for ASCII code */
            if( code == 0x0d || code == 0x0a ) {
                code = ' ' ;
            }
            fontdata = get_fontx2_a( (unsigned char *)DEF_FONT_A16_VAR, code );
            set_font( fontdata, (unsigned char *)dat_buffer, bitpos, DEF_FONT_SIZE/2 );
            bitpos += DEF_FONT_SIZE/2 ;
            pos++ ;
        } else {
            /* for KANJI code */
            num = utf8_to_sjis( &strbuff[ pos ], &code1, &code2 );

            code = (code1<<8) + code2 ;  /* get 2nd byte and marge */
            if( code >= 0xa100 && code <= 0xdf00 ) {
                code = 0x8140;
            }
            fontdata = get_fontx2_k( (unsigned char *)DEF_FONT_K16_VAR, code );
            set_font( fontdata, (unsigned char *)dat_buffer, bitpos, DEF_FONT_SIZE );
            bitpos += DEF_FONT_SIZE;
            pos += num;
        }

        if( bitpos >= (DEF_OUTDATA_NUM - DEF_FONT_SIZE - (DEF_MOD_WIDTH * DEF_MOD_NUM)) ) {
            break ;
        }
    }

    return  bitpos;
}


int  make_title( unsigned char *strbuff, unsigned int size )
{
    int  pos, bitpos;
    int  num;
    unsigned char  *fontdata;
    unsigned int  code;
    unsigned char  code1, code2 ;

    clear_title();

    pos = 0 ;
    bitpos = (DEF_MOD_WIDTH * DEF_MODT_NUM);

    while( pos < size ) {
        code = strbuff[ pos ] ;  /* get 1st byte */
        if( code < 0x80 ) {
            /* for ASCII code */
            if( code == 0x0d || code == 0x0a ) {
                code = ' ' ;
            }
            fontdata = get_fontx2_a( (unsigned char *)DEF_FONT_A16_VAR, code );
            set_font_title( fontdata, (unsigned char *)datt_buffer, bitpos, DEF_FONT_SIZE/2 );
            bitpos += DEF_FONT_SIZE/2 ;
            pos++ ;
        } else {
            /* for KANJI code */
            num = utf8_to_sjis( &strbuff[ pos ], &code1, &code2 );

            code = (code1<<8) + code2 ;  /* get 2nd byte and marge */
            if( code >= 0xa100 && code <= 0xdf00 ) {
                code = 0x8140;
            }
            fontdata = get_fontx2_k( (unsigned char *)DEF_FONT_K16_VAR, code );
            set_font_title( fontdata, (unsigned char *)datt_buffer, bitpos, DEF_FONT_SIZE );
            bitpos += DEF_FONT_SIZE;
            pos += num;
        }

        if( bitpos >= (DEF_OUTTITLE_NUM - DEF_FONT_SIZE - (DEF_MOD_WIDTH * DEF_MODT_NUM)) ) {
            break ;
        }
    }

    return  bitpos;
}


const char  *chks = "<title>";
const char  *chke = "</title>";

void  wifi_access( void )
{
    HTTPClient  http;
    int  httpCode;
    unsigned char  *src ;
    unsigned char  *dst ;
    unsigned char  *top;
    unsigned int  lens = strlen(chks);
    unsigned int  lene = strlen(chke);
    unsigned int  len;
    int  cnt;
    int  lim;
    int  i;
    int  mode;
    int  url_num;
    int  err_cnt = 0;
    const char *url_pnt;
    int  flag;

    digitalWrite( LEDPIN, HIGH );

    for( url_num=0 ; url_num<DEF_MSG_NUM_MAX ; url_num++ ) {

      url_pnt = url[ url_num ];
      if( url_pnt == NULL ) {
          Serial.printf("URL List check : url_num: %d\n", url_num );
          break;
      }

retry_loop:
      if( err_cnt > 3 ) {
          err_cnt = 0;
          continue;
      }
      http.begin( url_pnt );
      httpCode = http.GET();
      Serial.printf("URL: %s\n", url_pnt );
      Serial.printf("Response: %d", httpCode);
      Serial.println();
      if (httpCode == HTTP_CODE_OK) {
        String body = http.getString();
#if 0
        Serial.print("Response Body: ");
        Serial.println(body);
#endif

        src  = (unsigned char *)body.c_str();
        dst  = msgt_buffer[url_num];
        flag = 0;
        mode = 0;
        cnt = 0;
        lim = 0;

        while( cnt < DEF_CHAR_NUM && *src != 0x00 ) {
          if( ++lim > 10000 ) {
            Serial.printf("read data limit.");
            break;
          }
          if( *src != '<' ) {
            src++;
            continue;
          }
          if( mode == 0 ) {
            if( strncmp( chks, (const char*)src, (size_t)lens ) == 0 ) {
              mode = 1;
              src += lens;
              top = src;
            } else {
              src++ ;
            }
            continue;
          }
          if( mode == 1 ) {
            if( strncmp( chke, (const char*)src, (size_t)lene ) == 0) {
              len = (unsigned long)src - (unsigned long)top;
              for( i=0 ; i<len ; i++ ) {
                *dst++ = *top++;
              }

              if( flag == 0 ) {
                  led_msgt_buf_len[ url_num ] = len;
                  dst = msg_buffer[url_num];
                  flag = 1;
              } else {
                  *dst++ = ' '; /* set space */
                  *dst++ = ' '; /* set space */
                  src += lene;
                  cnt += len + 2;

                  *dst++ = ' '; /* set space */
                  *dst++ = ' '; /* set space */
                  cnt += 2;
              }
              mode = 0;
            }
          }
        }

        http.end();
        if( cnt == 0 ) {
          err_cnt++;
          goto retry_loop;
        }
        led_msg_buf_len[ url_num ] = cnt;
        *dst = 0x00;
        Serial.printf("title  (%d): %d\n", url_num, led_msgt_buf_len[ url_num ]);
        Serial.printf("%s\n", msgt_buffer[url_num] );
        Serial.printf("message(%d): %d\n", url_num, led_msg_buf_len[ url_num ]);
        Serial.printf("%s\n", msg_buffer[url_num] );

      } else {
        http.end();
      }
    }
    Serial.printf("url_num: %d\n", url_num );

    digitalWrite( LEDPIN, LOW );
}


void  wifi_init( void )
{
  digitalWrite( LEDPIN, HIGH );

  WiFi.disconnect(true);
  delay(1000);
  WiFiMulti wifiMulti;

  /* Wi-Fiアクセスポイントへの接続 */
  wifiMulti.addAP(ssid1, password1);
  wifiMulti.addAP(ssid2, password2);
  while(wifiMulti.run() != WL_CONNECTED) {
    vTaskDelay(100);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  digitalWrite( LEDPIN, LOW );
}


void  led_main( void *param )
{
  int  url_num;

  Serial.println( "call mainTask()" );

  led_msgt_len = 0;
  led_msgt_pos = 0 ;
  led_msg_len = 0;
  led_pos = 0;
  msg_reload_flag = 0;
  msg_chg_flag = 0;
  sys_init_flag = 1;

  clear_title();
  led_msg_len = make_message( (unsigned char *)msg_test, sizeof(msg_test) );
  Serial.printf( "data byte  = %d\n", sizeof(msg_test) );
  Serial.printf( "data width = %d\n", led_msg_len );

  wifi_init();

  while( 1 ) {
    wifi_access();
    sys_init_flag = 0;
    msg_reload_flag = 0;

    while( msg_reload_flag == 0 ) {
      for( url_num=0 ; url_num<DEF_MSG_NUM_MAX ; url_num++ ) {
        if( led_msg_buf_len[ url_num ] == 0 ) {
          continue;
        }
        led_msgt_pos = 0 ;
        led_msgt_len = make_title( msgt_buffer[ url_num ], led_msgt_buf_len[ url_num ] );
        Serial.printf("led_msgt_len(%d): %d\n", url_num, led_msgt_len);
        led_pos = 0 ;
        led_msg_len = make_message( msg_buffer[ url_num ], led_msg_buf_len[ url_num ] );
        Serial.printf("led_msg_len (%d): %d\n", url_num, led_msg_len);
        msg_chg_flag = 0;
        xEventGroupWaitBits( ehandMsg, 0x03, pdTRUE, pdFALSE, 100000 );

        if( msg_reload_flag != 0 ) {
          break;
        }
      }
    }
  }
}


void  led_slide( void *param )
{
  int  vol;
  int  title_flag = 0;

  Serial.println( "call slideTask()" );

  while( 1 ) {
    /* get speed volume */
    vol = analogRead( VOLPIN );
    if( vol < 200 ) {
      vol = 200;
    }
    vTaskDelay( vol>>7 );

#if 1
    if( digitalRead( BTNPIN ) == HIGH ) {
#else
    {
#endif

      if( title_flag == 0 ) {
        led_msgt_pos++ ;
      }
      if( led_msgt_pos >= led_msgt_len ) {
        led_msgt_pos = 0 ;
        if( led_msgt_len > (led_msg_len - led_pos)  ) {
          title_flag = 1;
        }
      }

      led_pos++ ;
      if( led_pos >= led_msg_len ) {
        led_pos = 0 ;
        title_flag = 0;
        if( sys_init_flag == 0 ) {
            msg_chg_flag = 1;
            xEventGroupSetBits( ehandMsg, 0x01 );
        }
      }
    }
  }
}


void  timer_message( void *param )
{
  msg_reload_flag = 1;
}


void setup( void )
{
  int  col, row;
  int  i, j;

  /* init serial */
  Serial.begin(115200);
  Serial.println( "call setup()" );

  pinMode( BTNPIN,  INPUT_PULLUP);
  pinMode( LEDPIN,  OUTPUT );
  digitalWrite( LEDPIN, LOW );

  pinMode( S1NPIN,  OUTPUT );
  pinMode( S2NPIN,  OUTPUT );
  pinMode( S3NPIN,  OUTPUT );
  pinMode( CLKPIN,  OUTPUT );
  pinMode( LATPIN,  OUTPUT );

  digitalWrite( S1NPIN, LOW );
  digitalWrite( S2NPIN, LOW );
  digitalWrite( S3NPIN, LOW );
  digitalWrite( CLKPIN, LOW );
  digitalWrite( LATPIN, HIGH );

    for( i=0 ; i<16 ; i++ ) {
        for( j=0 ; j<16 ; j++ ) {
            if( i == j ) {
                adr_tbl[i][j] = 1;
            } else {
                adr_tbl[i][j] = 0;
            }
        }
    }

  for( i=0 ; i<DEF_MSG_NUM_MAX ; i++ ) {
    led_msg_buf_len[i] = 0;
  }

  /* clear MATLED */
  dev_led_clr_out();

  ehandMsg = xEventGroupCreate();

  /* create task */
  xTaskCreatePinnedToCore( led_main,
                           "LED_MAIN",
                           0x2000,
                           NULL,
                           10,
                           &thand_mainTask,
                           0 );

  /* create task */
  xTaskCreatePinnedToCore( led_slide,
                           "LED_SLID",
                           0x2000,
                           NULL,
                           configMAX_PRIORITIES - 1,
                           &thand_slideTask,
                           0 );

  /* create timer */
  thand_timeMsg = xTimerCreate( "TIM_MSG",
                                DEF_MSG_UPDATE,
                                pdTRUE,
                                //pdFALSE,
                                NULL,
                                (TimerCallbackFunction_t)timer_message );

  xTimerStart( thand_timeMsg, 0 );
}


void loop( void )
{
  int  row, row2;
  int  col;
  int  num;
  int  line;
  int  l_led_pos;
  int  l_led_pos2;
  int  idx1;
  int  idx2;
  unsigned char  tmpchar ;
  unsigned char *buff2 = (unsigned char *)dat_buffer ;
  unsigned char *buff3 = (unsigned char *)datt_buffer ;

  if( msg_chg_flag != 0 ) {
    return;
  }

  l_led_pos = led_pos;
  l_led_pos2 = led_msgt_pos;
  for( line=0 ; line<16 ; line++ ) {

    row = DEF_OUTDATA_NUM * line + l_led_pos;
    for( num=DEF_MOD_NUM-1 ; num>=0 ; num-- ) {
      idx1 = row + DEF_MOD_WIDTH * num + DEF_LED_WIDTH - 1;
      idx2 = row + DEF_MOD_WIDTH * num + DEF_LED_WIDTH + DEF_LED_WIDTH - 1;

      for( col=0 ; col<DEF_LED_WIDTH ; col++ ) {
        digitalWrite( S1NPIN, adr_tbl[line][col] );
        digitalWrite( S2NPIN, buff2[ idx1 - col ] );
        digitalWrite( S3NPIN, buff2[ idx2 - col ] );
        usec_delay( 1 );
        digitalWrite( CLKPIN, 1 );
        usec_delay( 1 );
        digitalWrite( CLKPIN, 0 );
      }
    }

    row2 = DEF_OUTTITLE_NUM * line + l_led_pos2;
    for( num=DEF_MODT_NUM-1 ; num>=0 ; num-- ) {
      idx1 = row2 + DEF_MOD_WIDTH * num + DEF_LED_WIDTH - 1;
      idx2 = row2 + DEF_MOD_WIDTH * num + DEF_LED_WIDTH + DEF_LED_WIDTH - 1;

      for( col=0 ; col<DEF_LED_WIDTH ; col++ ) {
        digitalWrite( S1NPIN, adr_tbl[line][col] );
        digitalWrite( S2NPIN, buff3[ idx1 - col ] );
        digitalWrite( S3NPIN, buff3[ idx2 - col ] );
        usec_delay( 1 );
        digitalWrite( CLKPIN, 1 );
        usec_delay( 1 );
        digitalWrite( CLKPIN, 0 );
      }
    }

    digitalWrite( LATPIN, 0 );
    usec_delay( 1 );
    digitalWrite( LATPIN, 1 );
    usec_delay( 10 );

    for( num=DEF_MODT_NUM-1 ; num>=0 ; num-- ) {
      for( col=0 ; col<DEF_LED_WIDTH ; col++ ) {
        digitalWrite( S2NPIN, 0 );
        digitalWrite( S3NPIN, 0 );
        usec_delay( 1 );
        digitalWrite( CLKPIN, 1 );
        usec_delay( 1 );
        digitalWrite( CLKPIN, 0 );
      }
    }
  }

  dev_led_clr_out();
  usec_delay( 10 );
}

