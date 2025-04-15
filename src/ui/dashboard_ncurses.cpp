/**
 * @file dashboard_ncurses.cpp
 * @brief ncurses 기반 시스템 모니터링 대시보드 구현
 * @author 시스템 모니터링 팀
 */

#include "log/logger.h"
#include <term.h>
#include "ui/dashboard_ncurses.h"
#include "collectors/cpu_collector.h"
#include "collectors/memory_collector.h"
#include "collectors/disk_collector.h"
#include "collectors/systeminfo_collector.h"
#include "collectors/process_collector.h"
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <thread>
#include <ncursesw/curses.h>
#include <locale.h>
#include <stdlib.h>

/**
 * @brief DashboardNcurses 클래스의 생성자
 *
 * 멤버 변수들을 초기화합니다.
 */
DashboardNcurses::DashboardNcurses()
{
    // 생성자에서 멤버 변수 초기화
    row = 0;
    view_info = 0;
    current_page = 0;
    processes_per_page = 0;
    sort_by = 0;
}

/**
 * @brief DashboardNcurses 클래스의 소멸자
 *
 * ncurses 환경을 정리하고 종료합니다.
 */
DashboardNcurses::~DashboardNcurses()
{
    LOG_INFO("END NCURSES");
    cleanup();
}

/**
 * @brief ncurses 환경을 초기화합니다.
 *
 * 터미널 설정, 색상, 로케일 등의 기본 환경을 설정합니다.
 */
void DashboardNcurses::init()
{
    LOG_INFO("INIT NCURSES");
    // 환경 변수 설정
    putenv((char *)"NCURSES_NO_UTF8_ACS=1");
    putenv((char *)"LANG=ko_KR.UTF-8");
    putenv((char *)"LC_ALL=ko_KR.UTF-8");

    // 로케일 설정
    if (setlocale(LC_ALL, "ko_KR.UTF-8") == NULL)
    {
        fprintf(stderr, "UTF-8 로케일을 설정할 수 없습니다.\n");
        exit(1);
    }

    initscr();            // ncurses 초기화
    start_color();        // 색상 지원 활성화
    use_default_colors(); // 기본 색상 사용
    cbreak();             // line buffering 비활성화
    noecho();             // 키 입력 에코 비활성화
    keypad(stdscr, TRUE); // 특수 키 활성화
    curs_set(0);          // 커서 숨기기
    timeout(0);           // getch()를 non-blocking으로 설정 (100에서 0으로 변경)

    setterm("xterm-256color"); // 터미널 타입 설정

    // 색상 쌍 초기화
    init_pair(1, COLOR_CYAN, -1);    // 제목용 청록색
    init_pair(2, COLOR_GREEN, -1);   // 정상 상태용 녹색
    init_pair(3, COLOR_YELLOW, -1);  // 경고용 노란색
    init_pair(4, COLOR_RED, -1);     // 위험용 빨간색
    init_pair(5, COLOR_BLUE, -1);    // 정보용 파란색
    init_pair(6, COLOR_MAGENTA, -1); // 강조용 자주색
    LOG_INFO("INIT PAIR");
}

/**
 * @brief 화면을 지웁니다.
 */
void DashboardNcurses::clearScreen()
{
    LOG_INFO("CLEAR SCREEN");
    clear();
}

/**
 * @brief 시스템 모니터링 정보를 업데이트하고 화면에 표시합니다.
 *
 * CPU, 메모리, 디스크, 네트워크 정보를 수집하고 화면에 표시합니다.
 */
void DashboardNcurses::updateMonitor()
{
    LOG_INFO("UPDATE MONITOR");
    cpu_collector.collect();
    memory_collector.collect();
    disk_collector.collect();
    network_collector.collect();

    system_metrics.cpu = cpu_collector.getCpuInfo();
    system_metrics.memory = memory_collector.getMemoryInfo();
    system_metrics.disk = disk_collector.getDiskStats();
    system_metrics.network = network_collector.getInterfacesToVector();

    // 테두리 (청록색)
    attron(COLOR_PAIR(1));
    mvprintw(row++, 0, getDivider("시스템 모니터").c_str());
    attroff(COLOR_PAIR(1));
    row++;

    // CPU 정보 섹션
    attron(COLOR_PAIR(1));
    mvprintw(row++, 0, "[ CPU 정보 ]");
    attroff(COLOR_PAIR(1));

    // CPU 사용량에 따른 색상 변경
    int color = system_metrics.cpu.usage > 90 ? 4 : (system_metrics.cpu.usage > 70 ? 3 : 2);
    attron(COLOR_PAIR(color));
    mvprintw(row++, 0, "전체 CPU 사용량: %.1f%% %.1f°C", system_metrics.cpu.usage, system_metrics.cpu.temperature);
    attroff(COLOR_PAIR(color));

    for (size_t i = 0; i < system_metrics.cpu.cores.size(); i += 4)
    {
        std::string line;
        for (size_t j = 0; j < 4 && (i + j) < system_metrics.cpu.cores.size(); ++j)
        {
            char buf[50];
            snprintf(buf, sizeof(buf), "CPU%3zu: %5.1f%% %4.1f°C",
                     i + j, system_metrics.cpu.cores[i + j].usage, system_metrics.cpu.cores[i + j].temperature);
            line += buf;
            if (j < 3)
                line += " | ";
        }
        mvprintw(row++, 0, "%s", line.c_str());
    }
    row++;

    // 메모리 정보 섹션
    attron(COLOR_PAIR(1));
    mvprintw(row++, 0, "[ 메모리 정보 ]");
    attroff(COLOR_PAIR(1));

    double mem_percent = (system_metrics.memory.total > 0) ? (static_cast<double>(system_metrics.memory.used) / static_cast<double>(system_metrics.memory.total) * 100.0) : 0.0;

    // 메모리 사용량에 따른 색상 변경
    color = mem_percent > 90 ? 4 : (mem_percent > 70 ? 3 : 2);
    attron(COLOR_PAIR(color));
    mvprintw(row++, 0, "메모리 사용량: %zuMB / %zuMB (%.1f%%)",
             system_metrics.memory.used, system_metrics.memory.total, mem_percent);
    attroff(COLOR_PAIR(color));

    // 스왑 정보
    if (system_metrics.memory.swap_total > 0)
    {
        double swap_percent = static_cast<double>(system_metrics.memory.swap_used) / static_cast<double>(system_metrics.memory.swap_total) * 100.0;
        mvprintw(row++, 0, "스왑 사용량: %zuMB / %zuMB (%.1f%%)",
                 system_metrics.memory.swap_used, system_metrics.memory.swap_total, swap_percent);
    }
    row++;

    // 디스크 정보 섹션
    attron(COLOR_PAIR(1));
    mvprintw(row++, 0, "[ 디스크 정보 ]");
    attroff(COLOR_PAIR(1));

    for (const auto &disk : system_metrics.disk)
    {
        double usage_percent = (disk.total > 0) ? (static_cast<double>(disk.used) / static_cast<double>(disk.total) * 100.0) : 0.0;
        const char *device = disk.device.empty() ? "" : disk.device.c_str();
        mvprintw(row++, 0, "%-45s사용량: %6.1fGB / %6.1fGB (%4.1f%%)",
                 device,
                 (float)disk.used / 1024 / 1024 / 1024,
                 (float)disk.total / 1024 / 1024 / 1024,
                 usage_percent);
    }
    row++;

    // 네트워크 정보 섹션
    attron(COLOR_PAIR(1));
    mvprintw(row++, 0, "[ 네트워크 정보 ]");
    attroff(COLOR_PAIR(1));

    for (const auto &stats : system_metrics.network)
    {
        if (stats.interface != "lo")
        {
            const char *interface = stats.interface.empty() ? "" : stats.interface.c_str();
            mvprintw(row++, 0, "%-20s", (std::string(interface) + ":").c_str());
            mvprintw(row++, 2, "다운로드: %11.1f",
                     stats.rx_bytes_per_sec);
            mvprintw(row++, 2, "업로드:   %11.1f",
                     stats.tx_bytes_per_sec);
        }
    }
    row++;

    LOG_INFO("END UPDATE MONITOR");
}

/**
 * @brief 시스템 기본 정보를 업데이트하고 화면에 표시합니다.
 *
 * 호스트명, OS 정보, 커널 버전, 가동 시간 등을 화면에 표시합니다.
 */
void DashboardNcurses::updateSystemInfo()
{
    LOG_INFO("UPDATE SYSTEM INFO");
    systeminfo_collector.collect();
    const SystemInfo &sys_info = systeminfo_collector.getSystemInfo();

    // 시스템 정보 헤더 (청록색)
    attron(COLOR_PAIR(1));
    mvprintw(row++, 0, getDivider("시스템 정보").c_str());
    attroff(COLOR_PAIR(1));

    // 시스템 정보 내용 (파란색)
    attron(COLOR_PAIR(5));
    const char *hostname = sys_info.hostname.empty() ? "" : sys_info.hostname.c_str();
    const char *os_name = sys_info.os_name.empty() ? "" : sys_info.os_name.c_str();
    const char *kernel_version = sys_info.os_kernel_version.empty() ? "" : sys_info.os_kernel_version.c_str();

    mvprintw(row++, 0, "호스트명: %s", hostname);
    mvprintw(row++, 0, "운영체제: %s", os_name);
    mvprintw(row++, 0, "커널 버전: %s", kernel_version);

    // 가동 시간 계산
    long days = sys_info.uptime / (24 * 3600);
    long hours = (sys_info.uptime % (24 * 3600)) / 3600;
    long minutes = (sys_info.uptime % 3600) / 60;
    mvprintw(row++, 0, "가동 시간: %ld일 %ld시간 %ld분", days, hours, minutes);

    attroff(COLOR_PAIR(5));

    LOG_INFO("END UPDATE SYSTEM INFO");
}

/**
 * @brief 프로세스 정보를 업데이트하고 화면에 표시합니다.
 *
 * 실행 중인 프로세스 목록을 수집하고 페이지별로 표시합니다.
 * 정렬 기준에 따라 프로세스를 정렬하여 표시합니다.
 */
void DashboardNcurses::updateProcesses()
{
    LOG_INFO("UPDATE PROCESSES");

    process_collector.collect();
    const std::vector<ProcessInfo> &processes = process_collector.getProcesses(sort_by);

    // 프로세스 정보 헤더 (청록색)
    attron(COLOR_PAIR(1));
    mvprintw(row++, 0, getDivider("프로세스 정보").c_str());
    attroff(COLOR_PAIR(1));
    row++;

    // 정렬 기준 안내 추가
    mvprintw(row, 0, "정렬 기준 (↑↓): ");
    if (sort_by == 0)
    {
        attron(COLOR_PAIR(2));
        printw("CPU 사용량");
        attroff(COLOR_PAIR(2));
    }
    else
    {
        printw("CPU 사용량");
    }
    printw(" | ");
    if (sort_by == 1)
    {
        attron(COLOR_PAIR(2));
        printw("메모리 사용량");
        attroff(COLOR_PAIR(2));
    }
    else
    {
        printw("메모리 사용량");
    }
    printw(" | ");
    if (sort_by == 2)
    {
        attron(COLOR_PAIR(2));
        printw("PID");
        attroff(COLOR_PAIR(2));
    }
    else
    {
        printw("PID");
    }
    printw(" | ");
    if (sort_by == 3)
    {
        attron(COLOR_PAIR(2));
        printw("이름");
        attroff(COLOR_PAIR(2));
    }
    else
    {
        printw("이름");
    }
    row += 2;

    // 헤더 (자주색)
    attron(COLOR_PAIR(6));
    mvprintw(row++, 0, "%-7s %-25.25s %-10.10s %4s %6s%s %-8.8s %-40.40s",
             "PID", "processname", "user", "CPU%", "mem(MB)", "status", "command");
    attroff(COLOR_PAIR(6));

    // 한 페이지에 표시할 수 있는 프로세스 수 계산
    processes_per_page = static_cast<size_t>(LINES - row - 7);

    // 전체 페이지 수 계산
    size_t total_pages = (processes.size() + processes_per_page - 1) / processes_per_page;

    // 현재 페이지의 시작 인덱스와 끝 인덱스 계산
    size_t start_idx = current_page * processes_per_page;
    size_t end_idx = std::min(start_idx + processes_per_page, processes.size());

    // 현재 페이지의 프로세스만 표시
    for (size_t i = start_idx; i < end_idx; i++)
    {
        const auto &process = processes[i];
        // NULL 체크를 추가하여 안전하게 문자열 처리
        const char *name = process.name.empty() ? "" : process.name.c_str();
        const char *user = process.user.empty() ? "" : process.user.c_str();
        const char *status = process.status.empty() ? "" : process.status.c_str();
        const char *command = process.command.empty() ? "" : process.command.c_str();

        mvprintw(row++, 0, "%-7d %-25.25s %-10.10s %4.1f%% %6zu%s %-8.8s %-40.40s",
                 process.pid,
                 name,
                 user,
                 process.cpu_usage,
                 process.memory_rss,
                 status,
                 command);
    }

    row++;
    mvprintw(row++, 0, "페이지 %zu/%zu (← → 키로 페이지 이동)", current_page + 1, total_pages);

    LOG_INFO("END UPDATE PROCESSES");
}

/**
 * @brief 도커 컨테이너 정보를 업데이트하고 화면에 표시합니다.
 *
 * 실행 중인 도커 컨테이너 목록과 상세 정보를 화면에 표시합니다.
 */
void DashboardNcurses::updateDocker()
{
    LOG_INFO("UPDATE DOCKER");
    docker_collector.collect();
    const std::vector<DockerContainerInfo> &containers = docker_collector.getContainers();

    attron(COLOR_PAIR(1));
    mvprintw(row++, 0, getDivider("도커 컨테이너 정보").c_str());
    attroff(COLOR_PAIR(1));
    row++;

    mvprintw(row++, 0, "%-20s %-40s %-10s %-20s %-80s %-10s",
             "name", "image", "status", "created", "ports", "command");
    for (const auto &container : containers)
    {
        std::string ports_str;
        for (const auto &port : container.container_ports)
        {
            ports_str += port.host_port + "->" + port.container_port + " ";
        }

        // NULL 체크 추가
        const char *name = container.container_name.empty() ? "" : container.container_name.c_str();
        const char *image = container.container_image.empty() ? "" : container.container_image.c_str();
        const char *status = container.container_status.empty() ? "" : container.container_status.c_str();
        const char *created = container.container_created.empty() ? "" : container.container_created.c_str();
        const char *command = container.command.empty() ? "" : container.command.c_str();

        mvprintw(row++, 0, "%-20s %-40s %-10s %-20s %-80s %-10s",
                 name, image, status, created, ports_str.c_str(), command);
    }
    row++;

    LOG_INFO("END UPDATE DOCKER");
}

/**
 * @brief 전체 화면을 업데이트합니다.
 *
 * 현재 선택된 뷰에 따라 적절한 정보를 화면에 표시합니다.
 * 시간 정보와 안내 메시지도 함께 표시합니다.
 */
void DashboardNcurses::update()
{
    LOG_INFO("UPDATE");
    clearScreen();
    row = 0;

    // 현재 시간 표시 (청록색)
    attron(COLOR_PAIR(1));
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now_time));
    mvprintw(row++, 0, "현재 시간: %s", time_str);
    attroff(COLOR_PAIR(1));
    row++;

    if (view_info == 0)
    {
        updateMonitor();
    }
    else if (view_info == 1)
    {
        updateSystemInfo();
    }
    else if (view_info == 2)
    {
        updateProcesses();
    }
    else if (view_info == 3)
    {
        updateDocker();
    }

    // 하단 안내 메시지 (청록색)
    attron(COLOR_PAIR(1));
    mvprintw(row++, 0, getDivider().c_str());
    attroff(COLOR_PAIR(1));
    mvprintw(row++, 0, "종료하려면 '");
    attron(COLOR_PAIR(4)); // 빨간색으로 'q' 강조
    printw("q");
    attroff(COLOR_PAIR(4));
    printw("'를 누르세요. 시스템 정보는 '");
    attron(COLOR_PAIR(5)); // 파란색으로 'i' 강조
    printw("i");
    attroff(COLOR_PAIR(5));
    printw("', 모니터링은 '");
    attron(COLOR_PAIR(2)); // 녹색으로 'm' 강조
    printw("m");
    attroff(COLOR_PAIR(2));
    printw("', 프로세스 정보는 '");
    attron(COLOR_PAIR(6)); // 자주색으로 'p' 강조
    printw("p");
    attroff(COLOR_PAIR(6));
    printw("', 도커 컨테이너 정보는 '");
    attron(COLOR_PAIR(3)); // 노란색으로 'd' 강조
    printw("d");
    attroff(COLOR_PAIR(3));
    printw("'를 누르세요.");
    attroff(COLOR_PAIR(1));

    refresh();
    LOG_INFO("END UPDATE");
}

/**
 * @brief 사용자 입력을 처리합니다.
 *
 * 키보드 입력에 따라 다양한 동작을 수행합니다:
 * - q/Q: 프로그램 종료
 * - m/M: 모니터링 화면으로 전환
 * - i/I: 시스템 정보 화면으로 전환
 * - p/P: 프로세스 정보 화면으로 전환
 * - d/D: 도커 컨테이너 정보 화면으로 전환
 * - 화살표 키: 페이지 이동 및 정렬 변경
 */
void DashboardNcurses::handleInput()
{
    LOG_INFO("HANDLE INPUT");
    int ch = getch();
    if (ch == 'q' || ch == 'Q')
    {
        cleanup();
        running = false;
    }
    else if (ch == 'm' || ch == 'M')
    {
        view_info = 0;
        current_page = 0;
    }
    else if (ch == 'i' || ch == 'I')
    {
        view_info = 1;
        current_page = 0;
    }
    else if (ch == 'p' || ch == 'P')
    {
        view_info = 2;
        current_page = 0;
    }
    else if (ch == 'd' || ch == 'D')
    {
        view_info = 3;
        current_page = 0;
    }
    else if (ch == KEY_LEFT && view_info == 2)
    {
        if (current_page > 0)
            current_page--;
        else
        {
            const auto &processes = process_collector.getProcesses(sort_by);
            size_t total_pages = (processes.size() + processes_per_page - 1) / processes_per_page;
            current_page = total_pages - 1;
        }
    }
    else if (ch == KEY_RIGHT && view_info == 2)
    {
        const auto &processes = process_collector.getProcesses(sort_by);
        size_t total_pages = (processes.size() + processes_per_page - 1) / processes_per_page;
        if (current_page < total_pages - 1)
            current_page++;
        else
            current_page = 0;
    }
    else if (ch == KEY_UP && view_info == 2)
    {
        if (sort_by > 0)
            sort_by--;
        else
            sort_by = ProcessCollector::MAX_SORT_BY;
    }
    else if (ch == KEY_DOWN && view_info == 2)
    {
        if (sort_by < ProcessCollector::MAX_SORT_BY)
            sort_by++;
        else
            sort_by = 0;
    }
    LOG_INFO("END HANDLE INPUT");
}

/**
 * @brief ncurses 환경을 정리합니다.
 *
 * 터미널 설정을 원래대로 복원하고 ncurses를 종료합니다.
 */
void DashboardNcurses::cleanup()
{
    LOG_INFO("CLEANUP");
    // 표준 출력 버퍼 플러시
    fflush(stdout);

    // 커서 복원
    curs_set(1);

    // 터미널 모드 복원
    echo();
    nocbreak();
    nl(); // 개행 처리 복원
    keypad(stdscr, FALSE);

    // 화면 정리 및 종료
    refresh(); // 마지막 화면 상태 업데이트
    endwin();  // ncurses 종료

    // 추가 터미널 설정 복원
    system("stty echo"); // 터미널 에코 강제 복원
    LOG_INFO("END CLEANUP");
}

/**
 * @brief 구분선을 생성하여 반환합니다.
 *
 * @param title 구분선 중앙에 표시할 제목 (선택 사항)
 * @return std::string 생성된 구분선 문자열
 */
std::string DashboardNcurses::getDivider(const std::string &title)
{
    LOG_INFO("GET DIVIDER");
    int max_x = getmaxx(stdscr);
    std::string result;
    result.reserve(static_cast<std::string::size_type>(max_x * 3));

    if (!title.empty())
    {
        // 한글 문자열 길이 계산 - 안전하게 수정
        int title_length = 0;
        const char *ptr = title.c_str();
        while (*ptr)
        {
            if ((*ptr & 0x80) != 0 && *(ptr + 1) && *(ptr + 2)) // UTF-8 바이트 안전 체크
            {
                title_length += 2; // 한글은 2칸 차지
                ptr += 3;          // UTF-8 한글은 3바이트
            }
            else
            {
                title_length += 1; // ASCII 문자는 1칸
                ptr += 1;
            }
        }

        // 제목 위치 계산
        int padding = (max_x - title_length - 2) / 2;

        // 왼쪽 구분선
        for (int i = 0; i < padding; i++)
        {
            result += "═";
        }

        // 제목
        result += " " + title + " ";

        // 오른쪽 구분선
        int remaining = max_x - padding - title_length - 2;
        for (int i = 0; i < remaining; i++)
        {
            result += "═";
        }
    }
    else
    {
        // 제목이 없는 경우 전체를 구분선으로
        for (int i = 0; i < max_x; i++)
        {
            result += "═";
        }
    }

    LOG_INFO("END GET DIVIDER");
    return result;
}