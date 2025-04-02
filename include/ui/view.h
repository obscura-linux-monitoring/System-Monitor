#pragma once
#include <ncurses.h>

/**
 * @brief UI 구성요소의 기본 클래스
 *
 * View 클래스는 ncurses 기반 UI 컴포넌트의 추상 기본 클래스입니다.
 * 모든 UI 요소는 이 클래스를 상속받아 구현해야 합니다.
 */
class View
{
protected:
    /**
     * @brief 부모 윈도우 포인터
     */
    WINDOW *parent_win;

    /**
     * @brief 화면에서의 행 위치
     */
    int row;

    /**
     * @brief 화면에서의 열 위치
     */
    int col;

public:
    /**
     * @brief 기본 생성자
     *
     * 모든 멤버 변수를 기본값으로 초기화합니다.
     */
    View() : parent_win(nullptr), row(0), col(0) {}

    /**
     * @brief 부모 윈도우 설정
     *
     * @param win 설정할 부모 윈도우 포인터
     */
    void setWindow(WINDOW *win)
    {
        parent_win = win;
    }

    /**
     * @brief 초기화 함수
     *
     * 뷰의 초기 상태를 설정하는 순수 가상 함수입니다.
     * 모든 파생 클래스에서 반드시 구현해야 합니다.
     */
    virtual void init() = 0;

    /**
     * @brief 업데이트 함수
     *
     * 뷰의 내용을 업데이트하는 순수 가상 함수입니다.
     * 모든 파생 클래스에서 반드시 구현해야 합니다.
     */
    virtual void update() = 0;

    /**
     * @brief 크기 조정 함수
     *
     * 뷰의 크기를 조정하는 순수 가상 함수입니다.
     * 모든 파생 클래스에서 반드시 구현해야 합니다.
     */
    virtual void resize() = 0;

    /**
     * @brief 위치 설정 함수
     *
     * @param r 설정할 행 위치
     * @param c 설정할 열 위치
     */
    void setPosition(int r, int c)
    {
        row = r;
        col = c;
    }

    /**
     * @brief 뷰의 높이를 반환
     *
     * @return 뷰의 높이 (기본값: 1)
     */
    virtual int getHeight() const { return 1; }

    /**
     * @brief 가상 소멸자
     *
     * 상속 구조에서 적절한 리소스 해제를 위한 가상 소멸자입니다.
     */
    virtual ~View() = default;
};