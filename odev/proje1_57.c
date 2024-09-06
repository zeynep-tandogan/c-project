#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <SDL2/SDL_ttf.h>

// Bir polygon için verilecek nokta sayısının maksimum 20 olacağı varsayımını yaptık
#define MAX_POINTS_IN_A_POLYGON (20) 

// Siteden çekilen bilgide istenilen bir sayırda maksimum 10 polygon olabilir varsayımı yaptık
#define MAX_POLYGON_A_MAP (10) 

// Ekranda ölçeklenebilir şekilde harita bölgesinin iyi görülebilmesi her piksel 15 ile çarpıldı
#define PARCEL_UNIT_SIZE (15) 

// Ekranın bir hattında bulunacak nokta sayısına 55 kareden kaynaklı olacak şekilde 55 + 1 denildi
#define MAX_POINTS_A_LINE (55 + 1) 

// Screen width ve height alanı aynı olsun SCREEN_WH
#define SCREEN_WH ((MAX_POINTS_A_LINE - 1) * PARCEL_UNIT_SIZE)

// HTTP requestten gelecek maksimum response'u 5000 varsaydık
#define HTTP_MAX_RECV_DATA_LENGTH 5000

// 16 8 4 2 1
#define SQUARE_SIZES 5

#define MAX_SQUARE_IN_A_POLYGON 100

typedef struct 
{
    // noktaların tutulduğu dizi
    SDL_Point points[MAX_POINTS_IN_A_POLYGON];
    int count; // Gerçekte kaç tane nokta var
} Polygon;

typedef struct 
{
    // polygonların tutulduğu dizi
    Polygon polygons[MAX_POLYGON_A_MAP];
    int count; // Gerçekten kaç tane polygon var girdi textindeki bir line da
} AllPolygons;

typedef struct {
    int size;
    int x;
    int y;
} Square;

void initialize(void);
void deinitialize(void);

void draw_background(void);
void print_text(const char *p_text, SDL_Rect *p_location, SDL_Color *p_color);

bool get_informataion_from_web(int line, AllPolygons *p_polygons);
bool draw_filled_polygon(SDL_Renderer *p_renderer, const Polygon *p_polygon, const SDL_Color *p_color);

double polygon_area(const Polygon *p_polygon);

void calculate_collision_map(const AllPolygons *p_pollygons, int p_collision_map[][MAX_POINTS_A_LINE][MAX_POINTS_A_LINE]);
void find_min_max_points(const Polygon *p_polygon, int *p_min_x, int *p_min_y, int *p_max_x, int *p_max_y);

// Platform/sondaj alanları karelerden oluşmakta. Verilen polgon ile bu kareler bir nokta veya bir line 
// ile kesişse bile collision fonkisyonundan true değer döndürmekte
// 4 köşeli dikdörtgenler için özel bir fonksiyon yazıldı ki bu fonksiyon köşe ve kenarlarda oluşan çakışmaları almasın diye
// ikinci verilen polygon'u yani platform için oluşacak karesel alanları 1 er birim küçültmekte
bool detect_collision_poly_rect_without_corners(const Polygon *p_polygon1, const Polygon *parcel);
bool detect_collision_poly_poly(const Polygon *p_polygon1, const Polygon *p_polygon2);
bool detect_collision_poly_line(const Polygon *p_polygon, SDL_Point point1, SDL_Point point2);
bool detect_collision_line_line(SDL_Point point1, SDL_Point point2, SDL_Point point3, SDL_Point point4);
bool detect_collision_poly_point(const Polygon *p_polygon, SDL_Point point);

void cover_shape(int collision_map_for_one_poly[][MAX_POINTS_A_LINE], Square *p_squares, int *p_squares_count);

static SDL_Window *p_window;
static SDL_Renderer *p_renderer;
static TTF_Font *p_font1;

int main(int argc, char *args[])
{
    initialize();

    int selected_line = 0;
    int unit_drilling_cost = 0;
    double unit_platform_cost = 0.0;

    AllPolygons all_polygons = {0};
    
    // hesaplama için
    int collision_map[MAX_POLYGON_A_MAP][MAX_POINTS_A_LINE][MAX_POINTS_A_LINE] = {0};


    int screen = 0;
    char input_text[50] = {0};

    for (bool done = false; false == done;)
    {
        SDL_Event event;

        if (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT:
                    done = true;
                    break;
                case SDL_TEXTINPUT:
                    strcat(input_text, event.text.text);
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_RETURN)
                    {
                        float input_val = atof(input_text);
                        input_text[0] = 0;

                        if (selected_line <= 0)
                        {
                            selected_line = input_val;
                        }
                        else if (unit_drilling_cost > 10 || unit_drilling_cost < 1)
                        {
                            unit_drilling_cost = input_val;
                        }
                        else if (unit_platform_cost <= 0.0)
                        {
                            unit_platform_cost = input_val;
                        }
                    }
            }
        }

        // Arka plan + grid
        draw_background();

        // Kullanıcıdan bilgi isteme
        if (0 >= selected_line)
        {
            SDL_Rect location = { 5, 5, 0, 0 };
            SDL_Color color = { 0, 0, 0 };

            SDL_StartTextInput();
            print_text("LUTFEN GECERLI BIR INDEX GIRINIZ:", &location, &color);
        }
        else if (unit_drilling_cost > 10 || unit_drilling_cost < 1)
        {
            SDL_Rect location = { 5, 5, 0, 0 };
            SDL_Color color = { 139, 0, 0 };

            SDL_StartTextInput();
            print_text("LUTFEN GECERLI BIR SONDAJ MALIYETI GIRINIZ:", &location, &color);
        }
        else if (unit_platform_cost <= 0.0)
        {
            SDL_Rect location = { 5, 5, 0, 0 };
            SDL_Color color = { 0, 0, 128 };

            SDL_StartTextInput();
            print_text("LUTFEN BIR PLATFORM MALIYETI GIRINIZ:", &location, &color);
        }
        else if (0 == screen)
        {
            bool ret = get_informataion_from_web(selected_line, &all_polygons);

            if (true == ret)
            {
                screen = 1; // Bir daha buraya gelmemesi için ve indeksi lineda istememesi için diğer türlü sonsuz döngüye girme sıkıntısı

                SDL_StopTextInput();
            }
            else
            {
                selected_line = 0; // en baştan indeksi alır, indekste yanlışlık olabilir diye
            }
        }

        // Polygonlar
        
        double total_area = 0.0;

        for (int i = 0; i < all_polygons.count; i++)
        {
            SDL_Color color = { 139, 101, 139 };
            
            draw_filled_polygon(p_renderer, &all_polygons.polygons[i], &color);

            total_area += polygon_area(&all_polygons.polygons[i]);
        }

        if (screen == 1)
        {
            char text[50];

            sprintf(text, "Kaynak Rezerv Degeri : %.2f", total_area * 10);

            SDL_Rect location = { 5, 5, 0, 0 };
            SDL_Color color = { 0, 0, 0 };

            print_text(text, &location, &color);
        }
        
        int square_count = 0;

        if (screen == 2)
        {
            // Çakışmaların bilgileri çıkarılır
            calculate_collision_map(&all_polygons, collision_map);
             
          // collision_map kullanarak kareleri elde etmeliyiz
                    
         for (int i = 0; i < all_polygons.count; i++)
            {
                Square squares[MAX_SQUARE_IN_A_POLYGON];

                cover_shape(collision_map[i], squares, &square_count);
        
                for (int j = 0; j < square_count; j++)
                {
                    // farklı renkler size'a göre ayarlanıyor
                    SDL_SetRenderDrawColor(p_renderer, squares[j].size * 2, squares[j].size * 7, squares[j].size * 13, 0);

                    int x = squares[j].x * PARCEL_UNIT_SIZE;
                    int y = squares[j].y * PARCEL_UNIT_SIZE;
                    int dif = squares[j].size * PARCEL_UNIT_SIZE;
                    
                    SDL_Rect rect = {x, y, dif, dif};

                    SDL_RenderDrawRect(p_renderer, &rect);
                }
            }
        }
        // hesaplamalar

        if(screen == 3)
   
   
   {
        char text[50];
        SDL_Rect location;
        SDL_Color color = { 0, 0, 0};

        sprintf(text,"Toplam sondaj sayisi: %.2lf",total_area);
        location.x=10;
        location.y=10;
        print_text(text,&location,&color);

        sprintf(text,"Toplam sondaj maliyeti: %f",total_area * unit_drilling_cost);
        location.y=40;
        print_text(text,&location,&color);
        
        sprintf(text,"Toplam platform sayisi: %d",square_count);
        location.y=70;
        print_text(text,&location,&color);

        sprintf(text,"Toplam platform maliyeti: %f",square_count * unit_platform_cost);
        location.y=100;
        print_text(text,&location,&color);


        sprintf(text,"Toplam maliyeti: %f",square_count * unit_platform_cost + total_area*unit_drilling_cost);
        location.y=130;
        print_text(text,&location,&color);


        sprintf(text,"Kar miktari %f",total_area * 10- (square_count * unit_platform_cost + total_area*unit_drilling_cost));
        location.y=160;
        print_text(text,&location,&color);

}


        // Render

        SDL_RenderPresent(p_renderer);

        // 40 ms bekle

        SDL_Delay(40);

        // screen 1 gösterildikten sonra 5 sn ekranda bekler ve screen 2 olur.
        
        if (screen == 1)
        {
            screen = 2;
            SDL_Delay(3000); // 3 sn
        }
        else if(screen == 2)
        {
            screen = 3;
            SDL_Delay(5000);
        }
    }

    deinitialize();

    return 0;
}

void initialize(void)
{  
    SDL_Init(SDL_INIT_VIDEO);
    SDLNet_Init();
    TTF_Init();
    
    p_window = SDL_CreateWindow("REZERV BOLGELERİ", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WH, SCREEN_WH, SDL_WINDOW_SHOWN);
    p_renderer = SDL_CreateRenderer(p_window, -1, 0);
    p_font1 = TTF_OpenFont("free-sans.ttf", 20);
}

void deinitialize(void)
{
    TTF_CloseFont(p_font1);
    
    SDL_DestroyWindow(p_window);

    TTF_Quit();
    SDLNet_Quit();
    SDL_Quit();
}

void draw_background(void)
{
    // Arkaplan beyaz
    SDL_SetRenderDrawColor(p_renderer, 255, 255, 255, 0);
    SDL_RenderClear(p_renderer);
    
    SDL_Point grid_points[MAX_POINTS_A_LINE * 2];

    SDL_SetRenderDrawColor(p_renderer, 175, 175, 175, 0); // Gri olsun diye 175

    for (int i = 0; i <= SCREEN_WH; i += PARCEL_UNIT_SIZE)
    {
        float scale = 1;

        if (i % (5 * PARCEL_UNIT_SIZE) == 0)
        {
            scale = 2;
        }

        SDL_RenderSetScale(p_renderer, scale, scale);

        SDL_RenderDrawLine(p_renderer, i / scale, 0, i / scale, SCREEN_WH / scale);
        SDL_RenderDrawLine(p_renderer, 0, i / scale, SCREEN_WH / scale, i / scale);
    }
    
    SDL_RenderSetScale(p_renderer, 1, 1);
}

void print_text(const char *p_text, SDL_Rect *p_location, SDL_Color *p_color)
{
    SDL_Surface *p_text_surface = TTF_RenderText_Solid(p_font1, p_text, *p_color);
    
    SDL_Texture *p_message = SDL_CreateTextureFromSurface(p_renderer, p_text_surface);
    
    p_location->w = p_text_surface->w;
    p_location->h = p_text_surface->h;
    
    SDL_RenderCopy(p_renderer, p_message, NULL, p_location);

    SDL_DestroyTexture(p_message);
    SDL_FreeSurface(p_text_surface);
}

bool get_informataion_from_web(int line, AllPolygons *p_polygons)
{
    char recv_data[HTTP_MAX_RECV_DATA_LENGTH] = {0};
    const char send_data[] = {
        "GET /prolab1/prolab1.txt HTTP/1.1\r\n"
        "Host: bilgisayar.kocaeli.edu.tr\r\n"
        "\r\n"
    };

    IPaddress ip;
    SDLNet_ResolveHost(&ip, "bilgisayar.kocaeli.edu.tr", 80);

    TCPsocket socket = SDLNet_TCP_Open(&ip);
    SDLNet_SocketSet sockets = SDLNet_AllocSocketSet(1);
    
    int sent_length = SDLNet_TCP_Send(socket, send_data, sizeof(send_data));
    
    int recv_length;
    do
    {
        char buffer[1024];

        recv_length = SDLNet_TCP_Recv(socket, buffer, sizeof(buffer));

        strncat(recv_data, buffer, recv_length);
    }
    while (recv_length > 0);

    SDLNet_FreeSocketSet(sockets);
    SDLNet_TCP_Close(socket);

    // Debug
   // printf("recv_data : %s\n\n", recv_data);

    // HTTP response da content'imiz 2 tane \r\n den sonra gelir.
    // \r\n\r\n karakter dizilimini arrıyoruz ve bulduğumuz yerin 4
    // karakter sonrası contentimiz olacak.
    const char *p_start_address = strstr(recv_data, "\r\n\r\n") + 4;
    
    if (NULL == p_start_address)
    {
        printf("Hatali HTTP response\n");
        return false;
    }

    // ilgili satıra gelene kadar satırları atla
    // 1. satır için atlamaz. 1 DE DİREK 0
    
    while (--line)
    {
        p_start_address = strchr(p_start_address, '\n');

        if (NULL == p_start_address)
        {
            printf("Yanlis icerik veya gecersiz line\n");
            return false;
        }
        //yeni satıra gec
        p_start_address += 1;
    }

    // change \n to \0, bir sonraki ssatırlardaki noktaları bulmaması için
    char *p_end_line = strchr(p_start_address, '\n');

    if (NULL != p_end_line)
    {   
        *p_end_line = '\0';
    }

    // 10 ten azsa ve o polygon 20genden az kenarlıysa
    while ((p_polygons->count < MAX_POLYGON_A_MAP) && 
           (p_polygons->polygons[p_polygons->count].count < MAX_POINTS_IN_A_POLYGON))//kac tane polıgon(kacgen)
    {
        p_start_address = strchr(p_start_address + 1, '(');
        
        if (NULL == p_start_address)
            break;
        
        int x, y;

        sscanf(p_start_address, "(%d,%d)", &x , &y);
        
        int point_count = p_polygons->polygons[p_polygons->count].count;

        p_polygons->polygons[p_polygons->count].points[point_count].x = PARCEL_UNIT_SIZE * x;
        p_polygons->polygons[p_polygons->count].points[point_count].y = PARCEL_UNIT_SIZE * y;

        p_polygons->polygons[p_polygons->count].count++;

        if (point_count != 0) // veya bu koşul: p_polygons->polygons[p_polygons->count].count != 1
        {    //ilk nokta son nokta esit olma durumu
            if ((p_polygons->polygons[p_polygons->count].points[0].x == p_polygons->polygons[p_polygons->count].points[point_count].x) && 
                (p_polygons->polygons[p_polygons->count].points[0].y == p_polygons->polygons[p_polygons->count].points[point_count].y))
            {
                p_polygons->count++;
            }
        }
    }

    return true;
}

bool draw_filled_polygon(SDL_Renderer* p_renderer, const Polygon *p_polygon, const SDL_Color *p_color)
{
    int topY;            // üst köşenin y koordinatını tutmak için kullanılır
    int topCnt;            // üst köşenin indeksini tutmak için kullanılır
    int leftCnt;            // üst köşenin solundaki köşenin indeksini tutmak için kullanılır
    int rightCnt;           // Üst köşenin sağındaki köşenin indeksini tutmak için kullanılır
    int startX;             // Çizgiyi çizmek için başlangıç ​​noktası, matematiğini kullanır
    int endX;            // çizgiyi çizmek için bitiş noktası,  matematiğini kullanır
    int cntY;             //geçerli satırın y konumu
    int leftSlope;              // Sol tarafın eğimi, matematiğini kullanır
    int rightSlope;            //sağ tarafın eğimih
    int cnt;           // döngülerde kullanılan sayma değişkeni
    int numVerts = p_polygon->count;    // çizilen çokgenin köşe sayısı, hemen başlat
    int numVertsProc = 1;            // işlenen köşe sayısı, şu şekilde başlatılır:1

    SDL_Point center = {0};

    topY = p_polygon->points[0].y;          
    topCnt = 0;                      //en üst noktayı 0'a ayarla

   // gerçek en üst noktayı bul
    for(cnt = 1; cnt < numVerts; cnt++)      // çokgenin tüm köşeleri için
    {
        if(p_polygon->points[cnt].y < topY)       
        {                                     //eger daha kucukse daha yukardadır yeni topy 
            topY = p_polygon->points[cnt].y;        
            topCnt = cnt;           
        }
    }

    // soldaki noktayı bul
    leftCnt = topCnt - 1;           // sol noktayı üst noktadan bir eksik olarak ayarla
    if(leftCnt < 0)                // eğer belirtilen sol köşe 0'dan küçükse
        leftCnt = numVerts - 1;    //son eleamana gec

    //sağdaki noktayı bul
    rightCnt = topCnt + 1;          // sağ köşeyi üst köşeden bir fazlaya ayarladık
    if(rightCnt >= numVerts)        // Belirtilen sağ köşe noktası köşe sayısından fazlaysa
        rightCnt = 0;              // 0'a eşitleyelim

    startX = endX = (p_polygon->points[topCnt].x + center.x) << 16;   // çizginin başlangıç ​​ve bitiş noktalarını ayarlar, değer sola kaydırılır 
    cntY = p_polygon->points[topCnt].y;         //çizilecek ilk y değerini ayarla


    if(p_polygon->points[leftCnt].y != p_polygon->points[topCnt].y)     //üst ve sol köşe aynı y değerine sahip değilse
        leftSlope = ((p_polygon->points[leftCnt].x - p_polygon->points[topCnt].x) << 16) / (p_polygon->points[leftCnt].y - p_polygon->points[topCnt].y);    
    if(p_polygon->points[rightCnt].y != p_polygon->points[topCnt].y)   // üst ve sağ köşe aynı y değerine sahip değilse
        rightSlope = ((p_polygon->points[rightCnt].x - p_polygon->points[topCnt].x << 16)) / (p_polygon->points[rightCnt].y - p_polygon->points[topCnt].y); 

    SDL_SetRenderDrawColor(p_renderer, p_color->r, p_color->g, p_color->b, p_color->a);       

     //eğimi bul
    while(numVertsProc < numVerts)          //işlenecek köşeler kalırsa
    {
     // ilk noktaya kadar rasterleştir
        while(cntY < p_polygon->points[leftCnt].y && cntY < p_polygon->points[rightCnt].y)  
        {
            SDL_RenderDrawLine(p_renderer, startX >> 16, cntY + center.y, endX >> 16, cntY + center.y);
           //DrawLine(Point(startX, cntY + center.y), Point(endX, cntY + center.y), col);  // kenarlar arsı cigi çiz
            cntY++;            // y yi 1 arttır
            startX += leftSlope;         // başlangıç ​​x değerini sol eğim kadar arttır
            endX += rightSlope;       // bitiş x degerini sağ eğim kadar arttır
        }
        // üst noktayı buluştuğu noktaya ayarla
        // ayar noktası bir sonraki noktaya ulaştı
        // yeni eğimi bul
        if(p_polygon->points[leftCnt].y <= cntY)     // tarama çizgisi sol koordinatı bir sonraki noktaya geçerse
        {
            topCnt = leftCnt;    // üst noktayı sol tarafa ayarla
            leftCnt--;         // sol noktayı azalt
            if(leftCnt < 0)         // sol nokta 0 dan azsa
                leftCnt = numVerts - 1;     
            if(p_polygon->points[leftCnt].y != p_polygon->points[topCnt].y)    //kontrol
                leftSlope = ((p_polygon->points[leftCnt].x - p_polygon->points[topCnt].x) << 16) / (p_polygon->points[leftCnt].y - p_polygon->points[topCnt].y);   

            startX = (p_polygon->points[topCnt].x + center.x) << 16;    //başlangıç ​​x konumunu ayarla
            numVertsProc++;         // işlenen köşe sayısını arttır
        }

        if(p_polygon->points[rightCnt].y <= cntY)      // tarama çizgisi sağ koordinatı geçiyorsa
        {
            topCnt = rightCnt;         // üst noktayı sağ tarafa ayarla
            rightCnt++;          // sağ noktayı artır
            if(rightCnt == numVerts)      //sağ nokta köşe sayısından fazlaysa
                rightCnt = 0;        // sağ noktayı 0 a ayarla
            if(p_polygon->points[rightCnt].y != p_polygon->points[topCnt].y)  
                rightSlope = ((p_polygon->points[rightCnt].x - p_polygon->points[topCnt].x) << 16) / (p_polygon->points[rightCnt].y - p_polygon->points[topCnt].y); 

            endX = (p_polygon->points[topCnt].x + center.x) << 16;  //bitiş x i ayarla
            numVertsProc++;         // işlenen köşe sayısını arttır
        }
        SDL_RenderDrawLine(p_renderer, startX >> 16, cntY + center.y, endX >> 16, cntY + center.y);
        //DrawLine(Point(startX, cntY + center.y), Point(endX, cntY + center.y), col); // kenarlar arsına çizgi çiz 
    }
   

    return true;           
}

double polygon_area(const Polygon *p_polygon)
{   //denkleme göre
    double area = 0.0;
    
    int j = p_polygon->count - 1;

    for (int i = 0; i < p_polygon->count; i++)
    {
        area += (p_polygon->points[j].x + p_polygon->points[i].x) * (p_polygon->points[j].y - p_polygon->points[i].y) / (PARCEL_UNIT_SIZE * PARCEL_UNIT_SIZE * 1.0);
        j = i;
    }
 
    return abs(area / 2.0);
}

void calculate_collision_map(const AllPolygons *p_pollygons, int p_collision_map[][MAX_POINTS_A_LINE][MAX_POINTS_A_LINE])
{
    for (int i = 0; i < p_pollygons->count; i++)
    {
        int min_x, min_y, max_x, max_y;

        // fin min x,y and max x, y
        find_min_max_points(&p_pollygons->polygons[i], &min_x, &min_y, &max_x, &max_y);

        for (int j = min_y; j < max_y; j += PARCEL_UNIT_SIZE)
        {
            for (int k = min_x; k < max_x; k += PARCEL_UNIT_SIZE)
            {
                Polygon rect = {
                    {
                        {k, j}, {(k + PARCEL_UNIT_SIZE), j},
                        {(k + PARCEL_UNIT_SIZE), (j + PARCEL_UNIT_SIZE)}, {k, (j + PARCEL_UNIT_SIZE)},
                        {k, j}, // first point
                    }, 
                    5
                };

                // poly ile point arasında collison var mıy kontrol eden fonksiyon da
                // kullanılabilirdi herhalde. Lakin orada ek kontroller yapmak gerekirdi
                // performans düşünülmediğinden bu yöntem tercih edildi.
                bool collision = detect_collision_poly_rect_without_corners(&p_pollygons->polygons[i], &rect);

                p_collision_map[i][j / PARCEL_UNIT_SIZE][k / PARCEL_UNIT_SIZE] = (true == collision);
            }
        }
    }
}

void find_min_max_points(const Polygon *p_polygon, int *p_min_x, int *p_min_y, int *p_max_x, int *p_max_y)
{
    *p_min_x = *p_max_x = p_polygon->points[0].x;
    *p_min_y = *p_max_y = p_polygon->points[0].y;

    for (int i = 1; i < p_polygon->count; i++)
    {
        if (p_polygon->points[i].x > *p_max_x)
            *p_max_x = p_polygon->points[i].x;
        else if (p_polygon->points[i].x < *p_min_x)
            *p_min_x = p_polygon->points[i].x;
        if (p_polygon->points[i].y > *p_max_y)
            *p_max_y = p_polygon->points[i].y;
        else if (p_polygon->points[i].y < *p_min_y)
            *p_min_y = p_polygon->points[i].y;
    }
}

bool detect_collision_poly_rect_without_corners(const Polygon *p_polygon1, const Polygon *parcel)
{
    Polygon rect = *parcel;

    // Platformun karesel ypısını 1 er birim küçültüyoruz.
    // Çünkü köşelerde collision oluşuyor ve fazladan platform parselleri oluşuyor.

    rect.points[0].x += +1; rect.points[0].y += +1;
    rect.points[1].x += -1; rect.points[1].y += +1;
    rect.points[2].x += -1; rect.points[2].y += -1;
    rect.points[3].x += +1; rect.points[3].y += -1;
    rect.points[4].x += +1; rect.points[4].y += +1;

    return detect_collision_poly_poly(p_polygon1, &rect);
}

bool detect_collision_poly_poly(const Polygon *p_polygon1, const Polygon *p_polygon2) //2 =rect=parcel
{
    for (int i = 0; i < p_polygon1->count - 1; i++) // -1 çünkü bizim listede son nokta baştaki noktanın aynısı
    {   //ilk once kenarlar uzerinde kontrol
        bool collision = detect_collision_poly_line(p_polygon2, p_polygon1->points[i], p_polygon1->points[i + 1]);

        if (collision)
            return true;

        //içinde mi ve kesişme farkli seyler ilk once içindemi kontrol et
        collision = detect_collision_poly_point(p_polygon1, p_polygon2->points[0]);

        if (collision)
            return true;
    }

    return false;
}

//parcel ve  cokgen_kenar arası carpısma

bool detect_collision_poly_line(const Polygon *p_polygon, SDL_Point point1, SDL_Point point2)
{
    for (int current = 0; current < p_polygon->count - 1; current++)
    {
        bool hit = detect_collision_line_line(point1, point2, p_polygon->points[current], p_polygon->points[current + 1]);

        if (hit)
            return true;
    }

    return false;
}

bool detect_collision_line_line(SDL_Point point1, SDL_Point point2, SDL_Point point3, SDL_Point point4)
{
   // "çizgi çizgi çarpışma" algoritması
    //iki çizgiyi temsil eden parametrik denklemler
  //İlk çizgi: P1(x1, y1) + uA * (P2(x2, y2) - P1(x1, y1))
   //İkinci çizgi: P3(x3, y3) + uB * (P4(x4, y4) - P3(x3, y3)) 
    float uA = ((point4.x-point3.x)*(point1.y-point3.y) - (point4.y-point3.y)*(point1.x-point3.x)) / (float)((point4.y-point3.y)*(point2.x-point1.x) - (point4.x-point3.x)*(point2.y-point1.y));
    float uB = ((point2.x-point1.x)*(point1.y-point3.y) - (point2.y-point1.y)*(point1.x-point3.x)) / (float)((point4.y-point3.y)*(point2.x-point1.x) - (point4.x-point3.x)*(point2.y-point1.y));

   //yukardaki denklemlere gore u ların degerine gore noktalar araso konumlanıyoruz
    if (uA >= 0 && uA <= 1 && uB >= 0 && uB <= 1)
        return true;

    return false;
}
//polygonum ve parcelimin noktası "nokta polygonun içinde mi" algoritmaları 
bool detect_collision_poly_point(const Polygon *p_polygon, SDL_Point point)
{
    bool collision = false;

    for (int current = 0; current < p_polygon->count - 1; current++)
    {
        SDL_Point vc = p_polygon->points[current];   // current in c si
        SDL_Point vn = p_polygon->points[current + 1];   //next in n c i  

       // The point (point.x, point.y) is considered inside the polygon if the inequality is true and outside the polygon if the inequality is false 
        if (((vc.y >= point.y && vn.y < point.y) || (vc.y < point.y && vn.y >= point.y)) &&
            (point.x < (vn.x-vc.x)*(point.y-vc.y) / (vn.y-vc.y)+vc.x))
                collision = !collision;
    }

    return collision;
}

void cover_shape(int collision_map_for_one_poly[][MAX_POINTS_A_LINE], Square *p_squares, int *p_squares_count)
{
    int squareSizes[SQUARE_SIZES] = {16, 8, 4, 2, 1};

    for (int k = 0; k < SQUARE_SIZES; k++) {
        int squareSize = squareSizes[k];
        for (int i = 0; i < MAX_POINTS_A_LINE; i++) {
            for (int j = 0; j < MAX_POINTS_A_LINE; j++) {
                if (collision_map_for_one_poly[i][j] == 1) {
                    int valid = 1;
                    for (int x = i; x < i + squareSize; x++) {
                        for (int y = j; y < j + squareSize; y++) {
                            if (x >= MAX_POINTS_A_LINE || y >= MAX_POINTS_A_LINE || collision_map_for_one_poly[x][y] != 1) {
                                valid = 0;
                                break;
                            }
                        }
                        if (valid == 0) {
                            break;
                        }
                    }
                    if (valid) {
                        p_squares[*p_squares_count].size = squareSize;
                        p_squares[*p_squares_count].x = j;
                        p_squares[*p_squares_count].y = i;
                        (*p_squares_count)++;
                
                        for (int x = i; x < i + squareSize; x++) {
                            for (int y = j; y < j + squareSize; y++) {
                                collision_map_for_one_poly[x][y] = 0;
                            }
                        }
                        j -= squareSize;
                    }
                }
            }
        }
    }
}
