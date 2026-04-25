---
name: course-doc-review
description: Review the short-video recommender project docs before making, explaining, or reviewing changes. Use when working in E:\datastr on F1-F7 code, data files, algorithms, reports, or tests.
---

# Course Doc Review

## Purpose

Keep every coding or analysis task aligned with the course project documents instead of relying on memory.

## Required Start

Before changing code, generating data, reviewing code, or explaining a feature, read the relevant docs in `docs/`.

Always start with:

- `docs/assignment-requirements.md`

Then read the smallest relevant set:

- `docs/simulation-plan.md` for F1 video cleaning and F2 behavior simulation.
- `docs/data-schema.md` for CSV fields, intermediate structures, and output paths.
- `docs/algorithm-design.md` for data structures, algorithms, and complexity explanations.
- `docs/system-design.md` for module boundaries and data flow.
- `docs/report-outline.md` when preparing report or defense text.

## Working Rules

- State which F1-F7 requirement the task supports.
- Preserve project data unless the user explicitly asks to regenerate it.
- Prefer explainable data-structure methods over opaque libraries.
- For code changes, explain input, output, data structures, algorithm steps, and complexity when relevant.
- For data generation, verify row counts and key rates before saying the output is ready.

## F1/F2 Checklist

For F1 data construction:

- Confirm raw input path.
- Keep cleaned video output separate from simulated behavior output.
- Preserve `quality_score`, `topic_id`, `topic_vector`, and tag/category identifiers.

For F2 behavior simulation:

- Read cleaned videos instead of raw CSV.
- Generate at least 10000 users unless the user explicitly asks for a small sample.
- Keep behavior non-random enough to support F3-F7.
- Verify events obey `event_timestamp >= publish_ts`.
