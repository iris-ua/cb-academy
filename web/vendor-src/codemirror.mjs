// Re-exports the CodeMirror 6 pieces web/app.js uses, as window.CM.
export { basicSetup } from "codemirror";
export { EditorView, Decoration, keymap } from "@codemirror/view";
export { EditorState, StateField, StateEffect } from "@codemirror/state";
export { StreamLanguage } from "@codemirror/language";
export { indentWithTab } from "@codemirror/commands";
export { autocompletion, snippetCompletion } from "@codemirror/autocomplete";
export { oneDark } from "@codemirror/theme-one-dark";
export { lua } from "@codemirror/legacy-modes/mode/lua";
