/*
 * check_model.c — a tiny ONNX Runtime smoke-test program.
 *
 * Loads an ONNX model and prints whether it parsed correctly, plus its
 * input and output counts. Exits 0 on success, 1 on any failure. Suitable
 * as a CMD in a "model verifier" container that proves a packaged model
 * artifact still loads on the target runtime.
 *
 * You do not need to know C to do this lab — you compile this file with
 * gcc inside stage 1 of your Dockerfile and ship the resulting binary in
 * stage 2. Read the comments if you're curious.
 */

#include <stdio.h>
#include <onnxruntime_c_api.h>

int main(int argc, char** argv) {
    const char* model_path = (argc > 1) ? argv[1] : "/home/app/model.onnx";

    const OrtApi* api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (!api) {
        fprintf(stderr, "Failed to initialise ONNX Runtime API\n");
        return 1;
    }

    OrtEnv* env;
    OrtSessionOptions* opts;
    OrtSession* sess;
    OrtStatus* status;

    api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "verify", &env);
    api->CreateSessionOptions(&opts);

    status = api->CreateSession(env, model_path, opts, &sess);
    if (status != NULL) {
        fprintf(stderr, "FAILED to load: %s\n  %s\n",
                model_path, api->GetErrorMessage(status));
        api->ReleaseStatus(status);
        return 1;
    }

    size_t n_in = 0, n_out = 0;
    api->SessionGetInputCount(sess, &n_in);
    api->SessionGetOutputCount(sess, &n_out);

    printf("ONNX model loaded OK: %s\n", model_path);
    printf("  inputs:  %zu\n", n_in);
    printf("  outputs: %zu\n", n_out);

    api->ReleaseSession(sess);
    api->ReleaseSessionOptions(opts);
    api->ReleaseEnv(env);
    return 0;
}
