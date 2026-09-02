#include "airlabs_resolver.h"
#include "airport_board_fixture.h"
#include "airport_board_renderer.h"
#include "app_mode.h"
#include "animation.h"
#include "http_transport.h"
#include "flight_designator.h"
#include "input.h"
#include "layout.h"
#include "opensky_telemetry.h"
#include "provider.h"
#include "provider_debug.h"
#include "renderer.h"
#include "runtime.h"
#include "terminal.h"
#include "telemetry_history.h"
#include "visual_viewport.h"
#include "version.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile sig_atomic_t stop_requested = 0;

typedef struct {
    const char *flight_number;
    const char *date;
    bool use_fixture;
    bool use_board_fixture;
    FixtureKind fixture;
    AppMode mode;
    const char *airport_iata;
    AirportBoardDirection board_direction;
    VisualMode view;
    bool debug_provider;
    bool show_help;
    bool show_version;
} Options;

static void on_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static void sleep_frame(void)
{
    const struct timespec duration = { .tv_sec = 0, .tv_nsec = 50000000L };
    (void)nanosleep(&duration, NULL);
}

static void print_usage(FILE *output, const char *program)
{
    (void)fprintf(output,
        "usage: %s [FLIGHT] [OPTIONS]\n"
        "       %s --airport IATA [--arrivals|--departures] --fixture board\n"
        "\n"
        "options:\n"
        "  --date YYYY-MM-DD   constrain occurrence date\n"
        "  --fixture NAME      use deterministic mock data\n"
        "  --airport IATA      open fixture-backed AirportBoard mode\n"
        "  --arrivals          show airport arrivals\n"
        "  --departures        show airport departures (default)\n"
        "  --view NAME         select aircraft, altitude, or route\n"
        "  --debug-provider    print safe diagnostics after exit\n"
        "  --help              show this help\n"
        "  --version           show version\n"
        "\n"
        "controls: q quit, r refresh, v switch view, f next fixture, g geography (route)\n"
        "views: aircraft, altitude, route\n"
        "fixtures: cruising, scheduled, descending, landed, delayed, stale, unavailable, board\n",
        program, program);
}

static bool valid_date(const char *date)
{
    int index;
    if (strlen(date) != 10 || date[4] != '-' || date[7] != '-') return false;
    for (index = 0; index < 10; index++)
        if (index != 4 && index != 7 && (date[index] < '0' || date[index] > '9')) return false;
    return true;
}

static bool parse_arguments(int argc, char **argv, Options *options,
                            char *error, size_t error_capacity)
{
    int index;
    options->flight_number = "QF9";
    options->date = "";
    options->use_fixture = false;
    options->use_board_fixture = false;
    options->fixture = FIXTURE_QF9_CRUISING;
    options->mode = APP_MODE_FLIGHT;
    options->airport_iata = "";
    options->board_direction = AIRPORT_BOARD_DEPARTURES;
    options->view = VISUAL_AIRCRAFT;
    options->debug_provider = false;
    options->show_help = false;
    options->show_version = false;
    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--fixture") == 0) {
            if (++index >= argc) {
                (void)snprintf(error, error_capacity, "--fixture requires a valid fixture name");
                return false;
            }
            if (strcmp(argv[index], "board") == 0) options->use_board_fixture = true;
            else if (mock_provider_parse_fixture(argv[index], &options->fixture))
                options->use_fixture = true;
            else {
                (void)snprintf(error, error_capacity, "--fixture requires a valid fixture name");
                return false;
            }
        } else if (strcmp(argv[index], "--airport") == 0) {
            if (++index >= argc || strlen(argv[index]) != 3U) {
                (void)snprintf(error, error_capacity, "--airport requires a three-letter IATA code");
                return false;
            }
            options->mode = APP_MODE_AIRPORT;
            options->airport_iata = argv[index];
        } else if (strcmp(argv[index], "--arrivals") == 0) {
            options->board_direction = AIRPORT_BOARD_ARRIVALS;
        } else if (strcmp(argv[index], "--departures") == 0) {
            options->board_direction = AIRPORT_BOARD_DEPARTURES;
        } else if (strcmp(argv[index], "--date") == 0) {
            if (++index >= argc || !valid_date(argv[index])) {
                (void)snprintf(error, error_capacity, "--date requires YYYY-MM-DD");
                return false;
            }
            options->date = argv[index];
        } else if (strcmp(argv[index], "--view") == 0) {
            if (++index >= argc || !visual_mode_parse(argv[index], &options->view)) {
                (void)snprintf(error, error_capacity,
                               "--view requires aircraft, altitude, or route");
                return false;
            }
        } else if (strcmp(argv[index], "--debug-provider") == 0) {
            options->debug_provider = true;
        } else if (strcmp(argv[index], "--help") == 0) {
            options->show_help = true;
        } else if (strcmp(argv[index], "--version") == 0) {
            options->show_version = true;
        } else if (argv[index][0] == '-') {
            (void)snprintf(error, error_capacity, "unknown option: %s", argv[index]);
            return false;
        }
        else options->flight_number = argv[index];
    }
    if (options->mode == APP_MODE_AIRPORT && !options->use_board_fixture) {
        (void)snprintf(error, error_capacity,
                       "AirportBoard is fixture-only; use --fixture board");
        return false;
    }
    if (options->use_board_fixture && options->mode != APP_MODE_AIRPORT) {
        (void)snprintf(error, error_capacity, "--fixture board requires --airport IATA");
        return false;
    }
    return true;
}

static bool history_identity(char output[TELEMETRY_HISTORY_ID_CAPACITY],
                             const Options *options,
                             const LiveDataProviderContext *live_context)
{
    if (options->use_fixture) {
        (void)snprintf(output, TELEMETRY_HISTORY_ID_CAPACITY, "fixture:%s:%s",
                       options->flight_number, mock_provider_fixture_name(options->fixture));
        return true;
    }
    if (!live_context->have_resolved) return false;
    if (live_context->resolved.occurrence_id[0] == '\0' &&
        live_context->resolved.selected_leg.leg_id[0] == '\0') return false;
    (void)snprintf(output, TELEMETRY_HISTORY_ID_CAPACITY, "%s|%s",
                   live_context->resolved.occurrence_id,
                   live_context->resolved.selected_leg.leg_id);
    return output[0] != '\0';
}

static void collect_history(TelemetryHistory *history, const Options *options,
                            const LiveDataProviderContext *live_context,
                            const FlightState *flight)
{
    char identity[TELEMETRY_HISTORY_ID_CAPACITY];
    if (history_identity(identity, options, live_context))
        (void)telemetry_history_observe(history, identity, flight);
}

static uint64_t retry_delay(const FlightDataProvider *provider, ProviderResult result)
{
    if (result.status == PROVIDER_RATE_LIMITED && result.retry_after_seconds > 0)
        return (uint64_t)result.retry_after_seconds * UINT64_C(1000);
    if (result.status == PROVIDER_TIMEOUT || result.status == PROVIDER_UNAVAILABLE)
        return UINT64_C(60000);
    if (result.status == PROVIDER_AUTH_ERROR || result.status == PROVIDER_API_KEY_MISSING)
        return UINT64_C(3600000);
    return provider->refresh_interval_ms;
}

static int run_airport_board(const Options *options)
{
    AirportBoardState board;
    AppMode mode = APP_MODE_AIRPORT;
    FlightState opened_flight;
    TelemetryHistory opened_history;
    VisualViewport opened_viewport;
    AnimationState animation;
    RuntimeSchedule schedule;
    Terminal terminal;
    TerminalSize size;
    Layout layout;
    InputParser parser;
    bool running = true;
    bool redraw = true;
    int byte;
    if (!airport_board_fixture_load(&board, options->airport_iata)) {
        (void)fprintf(stderr, "flight: no AirportBoard fixture for %s\n",
                      options->airport_iata);
        return 2;
    }
    board.direction = options->board_direction;
    animation_init(&animation);
    runtime_schedule_init(&schedule, animation_now_ms());
    input_parser_init(&parser);
    telemetry_history_init(&opened_history);
    visual_viewport_init(&opened_viewport, VISUAL_AIRCRAFT, &opened_history);
    if (!terminal_install_resize_handler()) {
        airport_board_free(&board);
        (void)fputs("flight: could not install resize handler\n", stderr);
        return 1;
    }
    (void)signal(SIGINT, on_stop);
    (void)signal(SIGTERM, on_stop);
    if (!terminal_enter(&terminal)) {
        airport_board_free(&board);
        (void)fputs("flight: an interactive terminal is required\n", stderr);
        return 1;
    }
    terminal_set_mouse(&terminal, true);
    size = terminal_get_size();
    layout = layout_select(size);
    while (running && stop_requested == 0) {
        uint64_t now_ms = animation_now_ms();
        if (terminal_resize_pending()) {
            size = terminal_get_size();
            layout = layout_select(size);
            redraw = true;
        }
        while ((byte = terminal_read_key()) >= 0) {
            InputAction action;
            if (!input_parser_feed(&parser, byte, &action)) continue;
            if (action == INPUT_QUIT) running = false;
            else if (mode == APP_MODE_AIRPORT) {
                if (action == INPUT_BOARD_UP || action == INPUT_BOARD_DOWN) {
                    (void)airport_board_select_delta(&board,
                        action == INPUT_BOARD_UP ? -1 : 1,
                        airport_board_visible_capacity(&layout));
                    (void)airport_board_fixture_prefetch(&board);
                    redraw = true;
                } else if (action == INPUT_MOUSE && parser.mouse_pressed &&
                           airport_board_select_screen_row(&board, parser.mouse_row)) {
                    redraw = true;
                } else if (action == INPUT_BOARD_ARRIVALS ||
                           action == INPUT_BOARD_DEPARTURES) {
                    board.direction = action == INPUT_BOARD_ARRIVALS ?
                                      AIRPORT_BOARD_ARRIVALS : AIRPORT_BOARD_DEPARTURES;
                    redraw = true;
                } else if (action == INPUT_REFRESH) {
                    (void)airport_board_fixture_refresh(&board);
                    redraw = true;
                } else if (action == INPUT_BOARD_OPEN &&
                           airport_board_fixture_open_selected(&board, &opened_flight)) {
                    telemetry_history_init(&opened_history);
                    visual_viewport_init(&opened_viewport, VISUAL_AIRCRAFT, &opened_history);
                    terminal_set_mouse(&terminal, false);
                    mode = APP_MODE_FLIGHT;
                    redraw = true;
                }
            } else {
                if (action == INPUT_BACK) {
                    mode = APP_MODE_AIRPORT;
                    terminal_set_mouse(&terminal, true);
                    redraw = true;
                }
                else if (action == INPUT_REFRESH) {
                    mock_provider_refresh(&opened_flight, FIXTURE_QF9_CRUISING,
                                          board.local_now);
                    redraw = true;
                } else if (action == INPUT_NEXT_VISUAL) {
                    visual_viewport_toggle(&opened_viewport);
                    redraw = true;
                } else if (action == INPUT_TOGGLE_GEOGRAPHY &&
                           visual_viewport_toggle_geography(&opened_viewport)) redraw = true;
            }
        }
        if (runtime_animation_due(&schedule, now_ms) &&
            animation_update(&animation, now_ms)) redraw = true;
        if (runtime_render_due(&schedule, now_ms)) redraw = true;
        airport_board_expire_changes(&board, board.local_now);
        if (redraw) {
            if (mode == APP_MODE_AIRPORT) {
                Frame frame;
                airport_board_render(&frame, &board, &animation, &layout);
                renderer_present_frame(&frame, &layout);
            } else renderer_draw(&opened_flight, &animation, &layout, &opened_viewport);
            redraw = false;
        }
        sleep_frame();
    }
    terminal_leave(&terminal);
    airport_board_free(&board);
    return 0;
}

int main(int argc, char **argv)
{
    Options options;
    FlightState flight;
    FlightDataProvider provider;
    MockDataProviderContext mock_context;
    LiveDataProviderContext live_context = {0};
    FlightResolver resolver;
    TelemetryProvider telemetry;
    AirLabsResolverContext airlabs_context;
    OpenSkyTelemetryContext opensky_context;
    AnimationState animation;
    RuntimeSchedule schedule;
    TelemetryHistory history;
    VisualViewport viewport;
    Terminal terminal;
    TerminalSize size;
    Layout layout;
    ProviderResult provider_result;
    char argument_error[128] = "invalid arguments";
    bool running = true;
    bool redraw = true;
    int key;

    if (!parse_arguments(argc, argv, &options, argument_error, sizeof(argument_error))) {
        (void)fprintf(stderr, "flight: %s\n", argument_error);
        print_usage(stderr, argv[0]);
        return 2;
    }
    if (options.show_help) {
        print_usage(stdout, argv[0]);
        return 0;
    }
    if (options.show_version) {
        (void)printf("flight %s\n", FLIGHT_VERSION);
        return 0;
    }
    if (options.mode == APP_MODE_AIRPORT) return run_airport_board(&options);
    if (!options.use_fixture &&
        flight_designator_classify(options.flight_number) == DESIGNATOR_MALFORMED) {
        (void)fprintf(stderr, "flight: malformed commercial designator: %s\n",
                      options.flight_number);
        return 2;
    }
    if (!http_transport_init()) {
        (void)fputs("flight: could not initialize HTTP transport\n", stderr);
        return 1;
    }
    if (options.use_fixture) {
        provider_init_mock(&provider, &mock_context, options.fixture, options.flight_number);
    } else {
        airlabs_resolver_init(&resolver, &airlabs_context, getenv("AIRLABS_API_KEY"));
        opensky_telemetry_init(&telemetry, &opensky_context);
        provider_init_live(&provider, &live_context, resolver, telemetry,
                           options.flight_number, options.date);
    }
    provider_result = provider.load(provider.context, &flight, time(NULL));
    telemetry_history_init(&history);
    telemetry_history_set_interval(&history,
        (unsigned int)(provider.refresh_interval_ms / UINT64_C(1000)));
    collect_history(&history, &options, &live_context, &flight);
    animation_init(&animation);
    runtime_schedule_init(&schedule, animation_now_ms());
    visual_viewport_init(&viewport, options.view, &history);
    if (!terminal_install_resize_handler()) {
        (void)fputs("flight: could not install resize handler\n", stderr);
        http_transport_cleanup();
        return 1;
    }
    (void)signal(SIGINT, on_stop);
    (void)signal(SIGTERM, on_stop);
    if (!terminal_enter(&terminal)) {
        (void)fputs("flight: an interactive terminal is required\n", stderr);
        http_transport_cleanup();
        return 1;
    }

    size = terminal_get_size();
    layout = layout_select(size);
    while (running && stop_requested == 0) {
        uint64_t now_ms = animation_now_ms();
        if (terminal_resize_pending()) {
            size = terminal_get_size();
            layout = layout_select(size);
            redraw = true;
        }
        while ((key = terminal_read_key()) >= 0) {
            InputAction action = input_action_for_key(key);
            if (action == INPUT_QUIT) running = false;
            else if (action == INPUT_REFRESH) {
                provider_result = provider.refresh(provider.context, &flight, time(NULL));
                collect_history(&history, &options, &live_context, &flight);
                runtime_defer_data(&schedule, now_ms, retry_delay(&provider, provider_result));
                redraw = true;
            } else if (action == INPUT_NEXT_FIXTURE && options.use_fixture) {
                options.fixture = mock_provider_next_fixture(options.fixture);
                provider_init_mock(&provider, &mock_context, options.fixture, options.flight_number);
                provider_result = provider.load(provider.context, &flight, time(NULL));
                collect_history(&history, &options, &live_context, &flight);
                redraw = true;
            } else if (action == INPUT_NEXT_VISUAL) {
                visual_viewport_toggle(&viewport);
                redraw = true;
            } else if (action == INPUT_TOGGLE_GEOGRAPHY &&
                       visual_viewport_toggle_geography(&viewport)) {
                redraw = true;
            }
        }
        if (runtime_animation_due(&schedule, now_ms) &&
            animation_update(&animation, now_ms)) redraw = true;
        if (runtime_data_due(&schedule, now_ms, provider.refresh_interval_ms)) {
            provider_result = provider.refresh(provider.context, &flight, time(NULL));
            collect_history(&history, &options, &live_context, &flight);
            if (provider_result.status != PROVIDER_OK)
                runtime_defer_data(&schedule, now_ms, retry_delay(&provider, provider_result));
            redraw = true;
        }
        if (runtime_render_due(&schedule, now_ms)) redraw = true;
        if (redraw) {
            renderer_draw(&flight, &animation, &layout, &viewport);
            redraw = false;
        }
        sleep_frame();
    }
    terminal_leave(&terminal);
    if (options.debug_provider && !options.use_fixture)
        provider_debug_print(stderr, &live_context, &flight, provider_result);
    http_transport_cleanup();
    return 0;
}
