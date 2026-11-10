#pragma once
// Einzige externe Schnittstelle zur bisherigen Nerd-Wave-Laufzeit.
// Die zugehoerige CPP enthaelt bis zur schrittweisen Entkopplung
// weiterhin den bestehenden, eng verzahnten Runtime-Zustand.
void nerdWaveSetup();
void nerdWaveLoop();
