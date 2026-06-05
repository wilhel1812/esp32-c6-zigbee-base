#!/usr/bin/env node

const fs = require("fs");
const path = require("path");

const root = path.resolve(__dirname, "..");
const headerPath = path.join(root, "components", "esp32_c6_zigbee_base", "include", "esp32_c6_zigbee_base.h");
const metadataPath = path.join(root, "docs", "config_metadata.json");
const referencePath = path.join(root, "docs", "config-reference.md");
const mode = process.argv[2] || "check";

function loadMetadata() {
  return JSON.parse(fs.readFileSync(metadataPath, "utf8"));
}

function stripComments(text) {
  return text.replace(/\/\*[\s\S]*?\*\//g, "").replace(/\/\/.*$/gm, "");
}

function parsePublicConfigStructs() {
  const header = stripComments(fs.readFileSync(headerPath, "utf8"));
  const structs = new Map();
  const structRe = /typedef\s+struct\s*\{([\s\S]*?)\}\s*(esp32_c6_zigbee_base_\w*config_t)\s*;/g;
  let match;

  while ((match = structRe.exec(header)) !== null) {
    const body = match[1];
    const name = match[2];
    const fields = [];
    for (const rawLine of body.split("\n")) {
      const line = rawLine.trim();
      if (!line) continue;
      const fieldMatch = line.match(/([A-Za-z_][A-Za-z0-9_]*)\s*;$/);
      if (fieldMatch) {
        fields.push(fieldMatch[1]);
      }
    }
    structs.set(name, fields);
  }

  return structs;
}

function validateMetadata(metadata, structs) {
  const errors = [];
  const documented = new Map();

  for (const entry of metadata) {
    for (const key of ["struct", "field", "path", "type", "required", "default", "allowed", "description"]) {
      if (!(key in entry)) {
        errors.push(`metadata entry ${entry.path || entry.field || "<unknown>"} is missing ${key}`);
      }
    }
    const key = `${entry.struct}.${entry.field}`;
    if (documented.has(key)) {
      errors.push(`duplicate metadata for ${key}`);
    }
    documented.set(key, entry);
  }

  for (const [structName, fields] of structs) {
    for (const field of fields) {
      const key = `${structName}.${field}`;
      if (!documented.has(key)) {
        errors.push(`missing docs metadata for ${key}`);
      }
    }
  }

  for (const key of documented.keys()) {
    const [structName, field] = key.split(".");
    if (!structs.has(structName)) {
      errors.push(`metadata references missing struct ${structName}`);
    } else if (!structs.get(structName).includes(field)) {
      errors.push(`metadata references missing field ${key}`);
    }
  }

  return errors;
}

function groupByStruct(metadata) {
  const groups = new Map();
  for (const entry of metadata) {
    if (!groups.has(entry.struct)) {
      groups.set(entry.struct, []);
    }
    groups.get(entry.struct).push(entry);
  }
  return groups;
}

function escapeCell(value) {
  return String(value).replace(/\|/g, "\\|").replace(/\n/g, " ");
}

function generateReference(metadata) {
  const groups = groupByStruct(metadata);
  const lines = [
    "---",
    "title: Config Reference",
    "layout: default",
    "nav_order: 2",
    "---",
    "",
    "# Config Reference",
    "",
    "This page is generated from `docs/config_metadata.json`. Run `npm run docs:generate` after changing public product config fields.",
    "",
    "Product configs use a sectioned C shape inspired by Klipper configs: identity first, then board, Zigbee runtime settings, and feature sections.",
    "",
  ];

  for (const [structName, entries] of groups) {
    lines.push(`## ${structName}`, "");
    lines.push("| Field path | Type | Required | Default | Allowed values | Description |");
    lines.push("| --- | --- | --- | --- | --- | --- |");
    for (const entry of entries) {
      lines.push(`| \`${escapeCell(entry.path)}\` | \`${escapeCell(entry.type)}\` | ${entry.required ? "yes" : "no"} | ${escapeCell(entry.default)} | ${escapeCell(entry.allowed)} | ${escapeCell(entry.description)} |`);
    }
    lines.push("");
  }

  return `${lines.join("\n")}\n`;
}

const metadata = loadMetadata();
const structs = parsePublicConfigStructs();
const errors = validateMetadata(metadata, structs);

if (errors.length > 0) {
  console.error(errors.join("\n"));
  process.exit(1);
}

const generated = generateReference(metadata);
if (mode === "generate") {
  fs.writeFileSync(referencePath, generated);
  process.exit(0);
}

if (mode !== "check") {
  console.error(`unknown mode: ${mode}`);
  process.exit(1);
}

const existing = fs.existsSync(referencePath) ? fs.readFileSync(referencePath, "utf8") : "";
if (existing !== generated) {
  console.error("docs/config-reference.md is out of date. Run npm run docs:generate.");
  process.exit(1);
}
