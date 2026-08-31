#include "airlabs_resolver.h"
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
    FixtureKind fixture;
    AppMode mode;
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
        "\n"
        "options:\n"
        "  --date YYYY-MM-DD   constrain occurrence date\n"
        "  --fixture NAME      use deterministic mock data\n"
        "  --view NAME         select aircraft, altitude, route, or geo (experimental)\n"
        "  --debug-provider    print safe diagnostics after exit\n"
        "  --help              show this help\n"
        "  --version           show version\n"
        "\n"
        "controls: q quit, r refresh, v switch view, f next fixture (fixture mode)\n"
        "views: aircraft, altitude, route, geo (experimental; CLI only)\n"
        "fixtures: cruising, scheduled, descending, landed, delayed, stale, unavailable\n",
        program);
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
    options->fixture = FIXTURE_QF9_CRUISING;
    options->mode = APP_MODE_FLIGHT;
    options->view = VISUAL_AIRCRAFT;
    options->debug_provider = false;
    options->show_help = false;
    options->show_version = false;
    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--fixture") == 0) {
            if (++index >= argc || !mock_provider_parse_fixture(argv[index], &options->fixture)) {
                (void)snprintf(error, error_capacity, "--fixture requires a valid fixture name");
                return false;
            }
            options->use_fixture = true;
        } else if (strcmp(argv[index], "--date") == 0) {
            if (++index >= argc || !valid_date(argv[index])) {
                (void)snprintf(error, error_capacity, "--date requires YYYY-MM-DD");
                return false;
            }
            options->date = argv[index];
        } else if (strcmp(argv[index], "--view") == 0) {
            if (++index >= argc || !visual_mode_parse(argv[index], &options->view)) {
                (void)snprintf(error, error_capacity,
                               "--view requires aircraft, altitude, route, or geo");
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
    /* Airport mode is intentionally not CLI-reachable in V0.1. */
    if (options.mode != APP_MODE_FLIGHT) {
        http_transport_cleanup();
        return 2;
    }
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
